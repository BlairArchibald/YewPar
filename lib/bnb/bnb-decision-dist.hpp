#ifndef SKELETONS_BNB_DECISION_DIST_HPP
#define SKELETONS_BNB_DECISION_DIST_HPP

#include <vector>
#include <hpx/util/tuple.hpp>
#include <hpx/lcos/broadcast.hpp>

#include "registry.hpp"
#include "incumbent.hpp"

#include "workstealing/scheduler.hpp"
#include "workstealing/workqueue.hpp"

#include "util/doubleWritePromise.hpp"

namespace skeletons { namespace BnB { namespace Decision { namespace Dist {

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          typename ChildTask,
          bool PruneLevel>
void
expand(unsigned spawnDepth,
       const hpx::util::tuple<Sol, Bnd, Cand> & n,
       const hpx::naming::id_type foundProm) {
  constexpr bool const prunelevel = PruneLevel;

  auto reg = skeletons::BnB::Components::Registry<Space,Bnd>::gReg;
  if (reg->stopSearch_) {
    return;
  }

  auto newCands = Gen::invoke(0, reg->space_, n);

  std::vector<hpx::future<void> > childFuts;
  if (spawnDepth > 0) {
    childFuts.reserve(newCands.numChildren);
  }

  for (auto i = 0; i < newCands.numChildren; ++i) {
    auto c = newCands.next(reg->space_, n);
    auto lbnd = reg->localBound_.load();

    /* Prune if required */
    auto ubound = Bound::invoke(0, reg->space_, c);
    if (ubound < lbnd) {
      if (prunelevel) {
        break;
      } else {
        continue;
      }
    }

    /* Update incumbent if required */
    if (hpx::util::get<1>(c) == lbnd) {
      skeletons::BnB::Components::updateRegistryBound<Space, Bnd>(hpx::util::get<1>(c));
      hpx::lcos::broadcast<updateRegistryBound_act>(hpx::find_all_localities(), hpx::util::get<1>(c));

      typedef typename bounds::Incumbent<Sol, Bnd, Cand>::updateIncumbent_action act;
      hpx::async<act>(reg->globalIncumbent_, c).get();

      /* Found a solution, tell the main thread to kill everything */
      hpx::async<YewPar::util::DoubleWritePromise<bool>::set_value_action>(foundProm, true).get();
    }

    /* Search the child nodes */
    if (spawnDepth > 0) {
      hpx::lcos::promise<void> prom;
      auto pfut = prom.get_future();
      auto pid  = prom.get_id();

      ChildTask t;
      hpx::util::function<void(hpx::naming::id_type)> task;
      task = hpx::util::bind(t, hpx::util::placeholders::_1, spawnDepth - 1, c, pid, foundProm);
      hpx::apply<workstealing::workqueue::addWork_action>(workstealing::local_workqueue, task);

      childFuts.push_back(std::move(pfut));
    } else {
      expand<Space, Sol, Bnd, Cand, Gen, Bound, ChildTask, PruneLevel>(0, c, foundProm);
    }
  }

  if (spawnDepth > 0) {
    workstealing::tasks_required_sem.signal(); // Going to sleep, get more tasks
    hpx::wait_all(childFuts);
  }
}

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          typename ChildTask,
          bool PruneLevel = false>
void
doSearch(unsigned spawnDepth,
         hpx::util::tuple<Sol, Bnd, Cand> c,
         const hpx::naming::id_type foundProm,
         std::shared_ptr<hpx::promise<void> > done) {
  expand<Space, Sol, Bnd, Cand, Gen, Bound, ChildTask, PruneLevel>(spawnDepth, c, foundProm);

  // Search finished without finding a solution, wake up the main thread to finalise everything
  hpx::async<YewPar::util::DoubleWritePromise<bool>::set_value_action>(foundProm, true).get();
  done->set_value();
}

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          typename ChildTask,
          bool PruneLevel = false>
hpx::util::tuple<Sol, Bnd, Cand>
search(unsigned spawnDepth,
       const Space & space,
       const hpx::util::tuple<Sol, Bnd, Cand> & root,
       const Bnd & decisionBound) {

  // Initialise the registries on all localities
  auto inc = hpx::new_<bounds::Incumbent<Sol, Bnd, Cand> >(hpx::find_here()).get();
  hpx::wait_all(hpx::lcos::broadcast<initRegistry_act>(hpx::find_all_localities(), space, decisionBound, inc));

  // Initialise the global incumbent
  typedef typename bounds::Incumbent<Sol, Bnd, Cand>::updateIncumbent_action updateInc;
  hpx::async<updateInc>(inc, root).get();

  // Start work stealing schedulers on all localities
  std::vector<hpx::naming::id_type> workqueues;
  for (auto const& loc : hpx::find_all_localities()) {
    workqueues.push_back(hpx::new_<workstealing::workqueue>(loc).get());
  }
  hpx::wait_all(hpx::lcos::broadcast<startScheduler_action>(hpx::find_all_localities(), workqueues));


  // Setup early termination promise
  hpx::promise<int> foundProm;
  auto foundFut    = foundProm.get_future();
  auto foundPromId = foundProm.get_id();
  auto foundId = hpx::new_<YewPar::util::DoubleWritePromise<bool> >(hpx::find_here(), foundPromId).get();

  std::shared_ptr<hpx::promise<void> > donePromise = std::make_shared<hpx::promise<void> >();

  hpx::apply(hpx::util::bind(&doSearch<Space, Sol, Bnd, Cand, Gen, Bound, ChildTask, PruneLevel>, spawnDepth, root, foundId, donePromise));

  foundFut.get();

  // Signal for all searches to stop

  // FIXME: For some reason this isn't able to deduce the right type of
  // the template in the function which is why we (currently) pass the fake arg.
  hpx::wait_all(hpx::lcos::broadcast<setStopSearchFlag_act>(hpx::find_all_localities(), Bnd()));

  // Wait for all workqueues to flush before stopping them
  donePromise->get_future().get();
  hpx::wait_all(hpx::lcos::broadcast<stopScheduler_action>(hpx::find_all_localities()));

  // Read the result form the global incumbent
  typedef typename bounds::Incumbent<Sol, Bnd, Cand>::getIncumbent_action getInc;
  return hpx::async<getInc>(inc).get();
}

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          typename ChildTask,
          bool PruneLevel = false>
void
searchChildTask(unsigned spawnDepth,
                hpx::util::tuple<Sol, Bnd, Cand> c,
                hpx::naming::id_type p,
                const hpx::naming::id_type foundProm) {
  expand<Space, Sol, Bnd, Cand, Gen, Bound, ChildTask, PruneLevel>(spawnDepth, c, foundProm);
  workstealing::tasks_required_sem.signal();
  hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(p, true).get();
}

}}}}

#endif
