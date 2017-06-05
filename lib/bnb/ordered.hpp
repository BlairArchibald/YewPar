#ifndef SKELETONS_BNB_ORDERED_HPP
#define SKELETONS_BNB_ORDERED_HPP

#include <vector>
#include <hpx/util/tuple.hpp>
#include <hpx/lcos/broadcast.hpp>

#include "registry.hpp"
#include "incumbent.hpp"

#include "workstealing/priorityscheduler.hpp"
#include "workstealing/priorityworkqueue.hpp"

namespace skeletons { namespace BnB { namespace Ordered {

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
       const hpx::util::tuple<Sol, Bnd, Cand> & n) {
  constexpr bool const prunelevel = PruneLevel;

  auto reg = skeletons::BnB::Components::Registry<Space,Bnd>::gReg;

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
    if (ubound <= lbnd) {
      if (prunelevel) {
        break;
      } else {
        continue;
      }
    }

    /* Update incumbent if required */
    if (hpx::util::get<1>(c) > lbnd) {
      skeletons::BnB::Components::updateRegistryBound<Space, Bnd>(hpx::util::get<1>(c));
      hpx::lcos::broadcast<updateRegistryBound_act>(hpx::find_all_localities(), hpx::util::get<1>(c));

      typedef typename bounds::Incumbent<Sol, Bnd, Cand>::updateIncumbent_action act;
      hpx::async<act>(reg->globalIncumbent_, c).get();
    }

    /* Search the child nodes */
    if (spawnDepth > 0) {
      hpx::lcos::promise<void> prom;
      auto pfut = prom.get_future();
      auto pid  = prom.get_id();

      ChildTask t;
      hpx::util::function<void(hpx::naming::id_type)> task;
      task = hpx::util::bind(t, hpx::util::placeholders::_1, spawnDepth - 1, c, pid);
      hpx::apply<workstealing::priorityworkqueue::addWork_action>(workstealing::local_workqueue, 0, task);

      childFuts.push_back(std::move(pfut));
    } else {
      expand<Space, Sol, Bnd, Cand, Gen, Bound, ChildTask, PruneLevel>(0, c);
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
hpx::util::tuple<Sol, Bnd, Cand>
search(unsigned spawnDepth,
       const Space & space,
       const hpx::util::tuple<Sol, Bnd, Cand> & root) {

  // Initialise the registries on all localities
  auto bnd = hpx::util::get<1>(root);
  auto inc = hpx::new_<bounds::Incumbent<Sol, Bnd, Cand> >(hpx::find_here()).get();
  hpx::wait_all(hpx::lcos::broadcast<initRegistry_act>(hpx::find_all_localities(), space, bnd, inc));

  // Initialise the global incumbent
  typedef typename bounds::Incumbent<Sol, Bnd, Cand>::updateIncumbent_action updateInc;
  hpx::async<updateInc>(inc, root).get();

  // Start work stealing schedulers on all localities
  std::vector<hpx::naming::id_type> workqueues;
  for (auto const& loc : hpx::find_all_localities()) {
    workqueues.push_back(hpx::new_<workstealing::priorityworkqueue>(loc).get());
  }
  hpx::wait_all(hpx::lcos::broadcast<priority_startScheduler_action>(hpx::find_all_localities(), workqueues));

  expand<Space, Sol, Bnd, Cand, Gen, Bound, ChildTask, PruneLevel>(spawnDepth, root);

  // Stop all work stealing schedulers
  hpx::wait_all(hpx::lcos::broadcast<priority_stopScheduler_action>(hpx::find_all_localities()));

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
                hpx::naming::id_type p) {
  expand<Space, Sol, Bnd, Cand, Gen, Bound, ChildTask, PruneLevel>(spawnDepth, c);
  workstealing::tasks_required_sem.signal();
  hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(p, true).get();
}

}}}

#endif
