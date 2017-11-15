#ifndef SKELETONS_BNB_DECISION_DIST_HPP
#define SKELETONS_BNB_DECISION_DIST_HPP

#include <vector>
#include <hpx/util/tuple.hpp>
#include <hpx/lcos/broadcast.hpp>

#include "registry.hpp"
#include "incumbent.hpp"

#include "util/doubleWritePromise.hpp"

#include "workstealing/Scheduler.hpp"
#include "workstealing/policies/Workpool.hpp"

namespace skeletons { namespace BnB { namespace Dist {

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          bool PruneLevel = false>
struct BranchAndBoundSat {

  static hpx::future<void> createTask(const unsigned spawnDepth,
                                      const hpx::util::tuple<Sol, Bnd, Cand> & c,
                                      const hpx::naming::id_type foundProm) {
    hpx::lcos::promise<void> prom;
    auto pfut = prom.get_future();
    auto pid  = prom.get_id();

    ChildTask t;
    hpx::util::function<void(hpx::naming::id_type)> task;
    task = hpx::util::bind(t, hpx::util::placeholders::_1, spawnDepth, c, pid, foundProm);
    std::static_pointer_cast<Workstealing::Policies::Workpool>(Workstealing::Scheduler::local_policy)->addwork(task);
    return pfut;
  }

  static void expand(unsigned spawnDepth,
                     const hpx::util::tuple<Sol, Bnd, Cand> & n,
                     const hpx::naming::id_type foundProm,
                     std::vector<hpx::future<void> > & childFutures) {
    constexpr bool const prunelevel = PruneLevel;

    auto reg = skeletons::BnB::Components::Registry<Space, Sol, Bnd, Cand>::gReg;
    if (reg->stopSearch_) {
      return;
    }

    auto newCands = Gen::invoke(reg->space_, n);

    for (auto i = 0; i < newCands.numChildren; ++i) {
      auto c = newCands.next();
      auto lbnd = reg->localBound_.load();

      /* Prune if required */
      auto ubound = Bound::invoke(reg->space_, c);
      if (ubound < lbnd) {
        if (prunelevel) {
          break;
        } else {
          continue;
        }
      }

      /* Update incumbent if required */
      if (hpx::util::get<1>(c) == lbnd) {
        skeletons::BnB::Components::updateRegistryBound<Space, Sol, Bnd, Cand>(hpx::util::get<1>(c));
        hpx::lcos::broadcast<updateRegistryBound_act>(hpx::find_all_localities(), hpx::util::get<1>(c));

        typedef typename bounds::Incumbent<Sol, Bnd, Cand>::updateIncumbent_action act;
        hpx::async<act>(reg->globalIncumbent_, c).get();

        /* Found a solution, tell the main thread to kill everything */
        hpx::async<YewPar::util::DoubleWritePromise<bool>::set_value_action>(foundProm, true).get();
      }

      /* Search the child nodes */
      if (spawnDepth > 0) {
        auto pfut = createTask(spawnDepth - 1, c, foundProm);
        childFutures.push_back(std::move(pfut));
      } else {
        expand(0, c, foundProm, childFutures);
      }
    }
  }

  static void doSearch(unsigned spawnDepth,
           hpx::util::tuple<Sol, Bnd, Cand> c,
           const hpx::naming::id_type foundProm,
           std::shared_ptr<hpx::promise<void> > done) {
    std::vector<hpx::future<void> > childFutures;
    expand(spawnDepth, c, foundProm, childFutures);


    hpx::apply(hpx::util::bind([=](std::vector<hpx::future<void> > & futs) {
          hpx::wait_all(futs);
          // Search finished without finding a solution, wake up the main thread to finalise everything
          hpx::async<YewPar::util::DoubleWritePromise<bool>::set_value_action>(foundProm, true).get();
          done->set_value();
        }, std::move(childFutures)));
  }

  static hpx::util::tuple<Sol, Bnd, Cand> search(unsigned spawnDepth,
                                                 const Space & space,
                                                 const hpx::util::tuple<Sol, Bnd, Cand> & root,
                                                 const Bnd & decisionBound) {

    // Initialise the registries on all localities
    auto inc = hpx::new_<bounds::Incumbent<Sol, Bnd, Cand> >(hpx::find_here()).get();
    hpx::wait_all(hpx::lcos::broadcast<initRegistry_act>(hpx::find_all_localities(), space, decisionBound, inc, root));

    // Initialise the global incumbent
    typedef typename bounds::Incumbent<Sol, Bnd, Cand>::updateIncumbent_action updateInc;
    hpx::async<updateInc>(inc, root).get();

    // Start work stealing schedulers on all localities
    Workstealing::Policies::Workpool::initPolicy();

    // We launch n - 2 threads locally since this locality also calls doSearch as a scheduler thread
    auto allLocs = hpx::find_all_localities();
    allLocs.erase(std::remove(allLocs.begin(), allLocs.end(), hpx::find_here()), allLocs.end());

    auto threadCount = hpx::get_os_thread_count() == 1 ? 1 : hpx::get_os_thread_count() - 1;
    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::startSchedulers_act>(allLocs, threadCount));

    auto threadCountLocal = hpx::get_os_thread_count() == 1 ? 1 : hpx::get_os_thread_count() - 2;
    Workstealing::Scheduler::startSchedulers(threadCountLocal);

    // Setup early termination promise
    hpx::promise<int> foundProm;
    auto foundFut    = foundProm.get_future();
    auto foundPromId = foundProm.get_id();
    auto foundId = hpx::new_<YewPar::util::DoubleWritePromise<bool> >(hpx::find_here(), foundPromId).get();

    std::shared_ptr<hpx::promise<void> > donePromise = std::make_shared<hpx::promise<void> >();

    // Run the main funciton as a scheduler thread
    hpx::threads::executors::default_executor exe(hpx::threads::thread_priority_critical,
                                                  hpx::threads::thread_stacksize_huge);
    hpx::apply(exe, &Workstealing::Scheduler::scheduler, hpx::util::bind(&doSearch, spawnDepth, root, foundId, donePromise));

    foundFut.get();

    // Signal for all searches to stop
    // FIXME: For some reason this isn't able to deduce the right type of
    // the template in the function which is why we (currently) pass the fake arg.
    hpx::wait_all(hpx::lcos::broadcast<setStopSearchFlag_act>(hpx::find_all_localities(), Bnd()));

    // Wait for all workqueues to flush before stopping them
    donePromise->get_future().get();
    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::stopSchedulers_act>(hpx::find_all_localities()));

    // Read the result form the global incumbent
    typedef typename bounds::Incumbent<Sol, Bnd, Cand>::getIncumbent_action getInc;
    return hpx::async<getInc>(inc).get();
  }

  static void searchChildTask(unsigned spawnDepth,
                              hpx::util::tuple<Sol, Bnd, Cand> c,
                              hpx::naming::id_type p,
                              const hpx::naming::id_type foundProm) {
    std::vector<hpx::future<void> > childFutures;
    expand(spawnDepth, c, foundProm, childFutures);
    hpx::apply(hpx::util::bind([=](std::vector<hpx::future<void> > & futs) {
          hpx::wait_all(futs);
          hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(p, true);
        }, std::move(childFutures)));
  }

  struct ChildTask : hpx::actions::make_action<
    decltype(&BranchAndBoundSat<Space, Sol, Bnd, Cand, Gen, Bound, PruneLevel>::searchChildTask),
    &BranchAndBoundSat<Space, Sol, Bnd, Cand, Gen, Bound, PruneLevel>::searchChildTask,
    ChildTask>::type {};

};

}}}

#endif
