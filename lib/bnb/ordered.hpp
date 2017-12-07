#ifndef SKELETONS_BNB_ORDERED_HPP
#define SKELETONS_BNB_ORDERED_HPP

#include <vector>
#include <hpx/util/tuple.hpp>
#include <hpx/lcos/broadcast.hpp>

#include "registry.hpp"
#include "incumbent.hpp"

#include "workstealing/policies/PriorityOrdered.hpp"

namespace skeletons { namespace BnB { namespace Ordered {

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          bool PruneLevel = false>
struct BranchAndBoundOpt {
  // Tasks are accessed both sequentially and in parallel. We use promises to
  // avoid as much redundant computation as possible
  struct OrderedTask {
    OrderedTask(hpx::util::tuple<Sol, Bnd, Cand> n, unsigned priority) : node(n), priority(priority) {
      started.get_future();
      startedPromise = hpx::new_<YewPar::util::DoubleWritePromise<bool> >(hpx::find_here(), started.get_id()).get();
    }

    hpx::promise<bool> started;
    hpx::naming::id_type startedPromise; // YewPar::Util::DoubleWritePromise

    hpx::util::tuple<Sol, Bnd, Cand> node;
    unsigned priority;
  };

  // Spawn tasks in a discrepancy search fashion
  // Discrepancy search priority based on number of discrepancies taken
  // Invariant: spawnDepth > 0
  static std::vector<OrderedTask> prioritiseTasks(const Space & space,
                                                  unsigned spawnDepth,
                                                  const hpx::util::tuple<Sol, Bnd, Cand> & root) {
    std::vector<OrderedTask> tasks;

    std::function<void(unsigned, unsigned, const hpx::util::tuple<Sol, Bnd, Cand>&)>
        fn = [&](unsigned depth, unsigned numDisc, const hpx::util::tuple<Sol, Bnd, Cand>& n) {
      if (depth == 0) {
        tasks.push_back(std::move(OrderedTask(n, numDisc)));
      } else {
        auto newCands = Gen::invoke(space, n);
        for (auto i = 0; i < newCands.numChildren; ++i) {
          auto node = newCands.next();
          fn(depth - 1, numDisc + i, node);
        }
      }
    };

    fn(spawnDepth, 0, root);

    return tasks;
  }

  static void expand(const hpx::util::tuple<Sol, Bnd, Cand> & n) {
    constexpr bool const prunelevel = PruneLevel;

    auto reg = skeletons::BnB::Components::Registry<Space, Sol, Bnd, Cand>::gReg;

    auto newCands = Gen::invoke(reg->space_, n);

    for (auto i = 0; i < newCands.numChildren; ++i) {
      auto c = newCands.next();
      auto lbnd = reg->localBound_.load();

      /* Prune if required */
      auto ubound = Bound::invoke(reg->space_, c);
      if (ubound <= lbnd) {
        if (prunelevel) {
          break;
        } else {
          continue;
        }
      }

      /* Update incumbent if required */
      if (hpx::util::get<1>(c) > lbnd) {
        skeletons::BnB::Components::updateRegistryBound<Space, Sol, Bnd, Cand>(hpx::util::get<1>(c));
        hpx::lcos::broadcast<updateRegistryBound_act>(hpx::find_all_localities(), hpx::util::get<1>(c));

        typedef typename bounds::Incumbent<Sol, Bnd, Cand>::updateIncumbent_action act;
        hpx::async<act>(reg->globalIncumbent_, c).get();
      }

      expand(c);
    }
  }

  static hpx::util::tuple<Sol, Bnd, Cand> search(unsigned spawnDepth,
                                                 const Space & space,
                                                 const hpx::util::tuple<Sol, Bnd, Cand> & root) {

    // Initialise the registries on all localities
    auto bnd = hpx::util::get<1>(root);
    auto inc = hpx::new_<bounds::Incumbent<Sol, Bnd, Cand> >(hpx::find_here()).get();
    hpx::wait_all(hpx::lcos::broadcast<initRegistry_act>(hpx::find_all_localities(), space, bnd, inc, root));

    // Initialise the global incumbent
    typedef typename bounds::Incumbent<Sol, Bnd, Cand>::updateIncumbent_action updateInc;
    hpx::async<updateInc>(inc, root).get();

    if (spawnDepth == 0) {
      expand(root);
      typedef typename bounds::Incumbent<Sol, Bnd, Cand>::getIncumbent_action getInc;
      return hpx::async<getInc>(inc).get();
    }

    Workstealing::Policies::PriorityOrderedPolicy::initPolicy();

    auto tasks = prioritiseTasks(space, spawnDepth, root);
    for (auto const & t : tasks) {
      // Spawn the tasks
      ChildTask child;
      hpx::util::function<void(hpx::naming::id_type)> task;
      task = hpx::util::bind(child, hpx::util::placeholders::_1, t.node, t.startedPromise);
      std::static_pointer_cast<Workstealing::Policies::PriorityOrderedPolicy>
          (Workstealing::Scheduler::local_policy)->addwork(t.priority, std::move(task));
    }
    // We need at least 2 threads for this to work correctly
    auto allLocs = hpx::find_all_localities();
    allLocs.erase(std::remove(allLocs.begin(), allLocs.end(), hpx::find_here()), allLocs.end());

    auto threadCount = hpx::get_os_thread_count() == 1 ? 1 : hpx::get_os_thread_count() - 1;
    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::startSchedulers_act>(allLocs, threadCount));

    auto threadCountLocal = hpx::get_os_thread_count() == 1 ? 1 : hpx::get_os_thread_count() - 2;
    Workstealing::Scheduler::startSchedulers(threadCountLocal);

    // Sequential thread of execution
    for (auto & t : tasks) {
      auto weStarted = hpx::async<YewPar::util::DoubleWritePromise<bool>::set_value_action>(t.startedPromise, true).get();
      if (weStarted) {
        expand(t.node);
      }
    }

    // Stop all work stealing schedulers
    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::stopSchedulers_act>(hpx::find_all_localities()));

    // Read the result form the global incumbent
    typedef typename bounds::Incumbent<Sol, Bnd, Cand>::getIncumbent_action getInc;
    return hpx::async<getInc>(inc).get();
}

  static void searchChildTask(hpx::util::tuple<Sol, Bnd, Cand> c,
                              const hpx::naming::id_type started) {
    auto weStarted = hpx::async<YewPar::util::DoubleWritePromise<bool>::set_value_action>(started, true).get();
    // Sequential thread has beaten us to this task. Don't bother executing it again.
    if (weStarted) {
      expand(c);
    }
  }

  struct ChildTask : hpx::actions::make_action<
    decltype(&BranchAndBoundOpt<Space, Sol, Bnd, Cand, Gen, Bound, PruneLevel>::searchChildTask),
    &BranchAndBoundOpt<Space, Sol, Bnd, Cand, Gen, Bound, PruneLevel>::searchChildTask,
    ChildTask>::type {};


};

}}}

#endif
