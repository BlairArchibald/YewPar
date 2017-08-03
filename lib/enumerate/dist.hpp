#ifndef SKELETONS_ENUM_DIST_HPP
#define SKELETONS_ENUM_DIST_HPP

#include <vector>
#include <hpx/util/tuple.hpp>
#include <hpx/lcos/broadcast.hpp>
#include <cstdint>

#include "registry.hpp"
#include "counter.hpp"

#include "workstealing/scheduler.hpp"
#include "workstealing/workqueue.hpp"

/* Counts the number of leaf search nodes. Up to the user to add depth bounding if required:
   e.g Store the depth in Sol and return no children once it is reached */

namespace skeletons { namespace Enum { namespace Dist {

template <typename Space,
          typename Sol,
          typename Gen,
          typename ChildTask>
expand(unsigned spawnDepth,
       const Sol & n
       std::uint64_t & cnt) {
  auto reg = skeletons::Enum::Components::Registry<Space, Sol>::gReg;

  auto newCands = Gen::invoke(0, reg->space_, n);

  std::vector<hpx::future<void> > childFuts;
  if (spawnDepth > 0) {
    childFuts.reserve(newCands.numChildren);
  }

  for (auto i = 0; i < newCands.numChildren; ++i) {
    auto c = newCands.next(reg->space_, n);

    /* Search the child nodes */
    if (spawnDepth > 0) {
      hpx::lcos::promise<void> prom;
      auto pfut = prom.get_future();
      auto pid  = prom.get_id();

      ChildTask t;
      hpx::util::function<void(hpx::naming::id_type)> task;
      task = hpx::util::bind(t, hpx::util::placeholders::_1, spawnDepth - 1, c, pid);
      hpx::apply<workstealing::workqueue::addWork_action>(workstealing::local_workqueue, task);

      childFuts.push_back(std::move(pfut));
    } else {
      expand<Space, Sol, Gen, ChildTask>(0, c);
    }
  }

  if (spawnDepth > 0) {
    workstealing::tasks_required_sem.signal(); // Going to sleep, get more tasks
    hpx::wait_all(childFuts);
  }

  return 0;
}

template <typename Space,
          typename Sol,
          typename Cand,
          typename Gen,
          typename ChildTask>
hpx::util::tuple<Sol, Cand>
search(unsigned spawnDepth,
       const Space & space,
       const hpx::util::tuple<Sol, Cand> & root) {

  // TODO: Allow this component take any numerical type
  auto counter = hpx::new_<Enum::Counter>(hpx::find_here()).get();

  hpx::wait_all(hpx::lcos::broadcast<enum_initRegistry_act>(hpx::find_all_localities(), space, counter, root));

  std::vector<hpx::naming::id_type> workqueues;
  for (auto const& loc : hpx::find_all_localities()) {
    workqueues.push_back(hpx::new_<workstealing::workqueue>(loc).get());
  }
  hpx::wait_all(hpx::lcos::broadcast<startScheduler_action>(hpx::find_all_localities(), workqueues));

  std::uint64_t cnt;
  expand<Space, Sol, Cand, Gen, ChildTask>(spawnDepth, root, cnt);

  // Add the count of the "main" thread (since this doesn't return the same way the other tasks do)
  auto reg = skeletons::Enum::Components::Registry<Space, Sol, Cand>::gReg;
  hpx::async<enum_add_action>(reg->globalCounter_, cnt).get();

  workstealing::tasks_required_sem.signal();
  hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(p, true).get();

}

}}}

#endif
