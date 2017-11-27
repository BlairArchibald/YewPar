#ifndef SKELETONS_BNB_DIST_RECOMPUTE_HPP
#define SKELETONS_BNB_DIST_RECOMPUTE_HPP

#include <vector>
#include <hpx/util/tuple.hpp>
#include <hpx/lcos/broadcast.hpp>

#include "registry.hpp"
#include "incumbent.hpp"

#include "workstealing/Scheduler.hpp"
#include "workstealing/policies/Workpool.hpp"

// Same as the depth-bounded skeleton but uses computation instead of sending
// full node structures. This lets us assess the trade-off between computation
// and communication
namespace skeletons { namespace BnB { namespace DistRecompute {

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          bool PruneLevel = false>
struct BranchAndBoundOpt {

  static hpx::future<void> createTask(const unsigned spawnDepth,
                                      const std::vector<unsigned> & path) {
    hpx::lcos::promise<void> prom;
    auto pfut = prom.get_future();
    auto pid  = prom.get_id();

    ChildTask t;
    hpx::util::function<void(hpx::naming::id_type)> task;
    task = hpx::util::bind(t, hpx::util::placeholders::_1, spawnDepth, path, pid);
    std::static_pointer_cast<Workstealing::Policies::Workpool>(Workstealing::Scheduler::local_policy)->addwork(task);
    return pfut;
  }

  static void expand(unsigned spawnDepth,
                     const hpx::util::tuple<Sol, Bnd, Cand> & n,
                     std::vector<unsigned> & path,
                     std::vector<hpx::future<void> > & childFutures) {
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

      path.push_back(i);

      /* Search the child nodes */
      if (spawnDepth > 0) {
        auto pfut = createTask(spawnDepth - 1, path);
        childFutures.push_back(std::move(pfut));
      } else {
        expand(0, c, path, childFutures);
      }

      path.pop_back();
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

    // Start work stealing schedulers on all localities
    Workstealing::Policies::Workpool::initPolicy();

    auto threadCount = hpx::get_os_thread_count() == 1 ? 1 : hpx::get_os_thread_count() - 1;
    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::startSchedulers_act>(hpx::find_all_localities(), threadCount));

    std::vector<unsigned> path;
    createTask(spawnDepth, path).get();

    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::stopSchedulers_act>(hpx::find_all_localities()));

    // Read the result form the global incumbent
    typedef typename bounds::Incumbent<Sol, Bnd, Cand>::getIncumbent_action getInc;
    return hpx::async<getInc>(inc).get();
  }

  static hpx::util::tuple<Sol, Bnd, Cand> getStartingNode(std::vector<unsigned> & path) {
    auto reg = skeletons::BnB::Components::Registry<Space, Sol, Bnd, Cand>::gReg;
    auto node =  reg->root_;

    for (auto const & p : path) {
      auto newCands = Gen::invoke(reg->space_, node);
      node = newCands.nth(p);
    }

    return node;
  }

  static void searchChildTask(unsigned spawnDepth,
                              std::vector<unsigned> path,
                              hpx::naming::id_type p) {
    auto c = getStartingNode(path);

    std::vector<hpx::future<void> > childFutures;
    expand(spawnDepth, c, path, childFutures);
    hpx::apply(hpx::util::bind([=](std::vector<hpx::future<void> > & futs) {
          hpx::wait_all(futs);
          hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(p, true);
        }, std::move(childFutures)));
  }

  struct ChildTask : hpx::actions::make_action<
    decltype(&BranchAndBoundOpt<Space, Sol, Bnd, Cand, Gen, Bound, PruneLevel>::searchChildTask),
    &BranchAndBoundOpt<Space, Sol, Bnd, Cand, Gen, Bound, PruneLevel>::searchChildTask,
    ChildTask>::type {};
};

}}}

#endif
