#ifndef SKELETONS_ENUM_DIST_HPP
#define SKELETONS_ENUM_DIST_HPP

#include <vector>
#include <hpx/util/tuple.hpp>
#include <hpx/lcos/broadcast.hpp>
#include <cstdint>

#include "enumRegistry.hpp"

#include "workstealing/scheduler.hpp"
#include "workstealing/workqueue.hpp"

namespace skeletons { namespace Enum { namespace Dist {

template <typename Space,
          typename Sol,
          typename Gen,
          typename ChildTask>
void expand(unsigned spawnDepth,
            const unsigned maxDepth,
            unsigned depth,
            const Sol & n,
            std::vector<std::uint64_t> & cntMap) {
  auto reg = Components::Registry<Space, Sol>::gReg;

  auto newCands = Gen::invoke(0, reg->space_, n);

  std::vector<hpx::future<void> > childFuts;
  if (spawnDepth > 0) {
    childFuts.reserve(newCands.numChildren);
  }

  cntMap[depth] += newCands.numChildren;

  if (maxDepth == depth) {
    return;
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
      task = hpx::util::bind(t, hpx::util::placeholders::_1, spawnDepth - 1, maxDepth, depth + 1, c, pid);
      hpx::apply<workstealing::workqueue::addWork_action>(workstealing::local_workqueue, task);

      childFuts.push_back(std::move(pfut));
    } else {
      expand<Space, Sol, Gen, ChildTask>(0, maxDepth, depth + 1, c, cntMap);
    }
  }

  if (spawnDepth > 0) {
    workstealing::tasks_required_sem.signal(); // Going to sleep, get more tasks
    hpx::wait_all(childFuts);
  }
}

template <typename Space,
          typename Sol,
          typename Gen,
          typename ChildTask>
std::vector<std::uint64_t> count(unsigned spawnDepth,
                                 const unsigned maxDepth,
                                 const Space & space,
                                 const Sol   & root) {
  hpx::wait_all(hpx::lcos::broadcast<enum_initRegistry_act>(hpx::find_all_localities(), space, maxDepth, root));

  std::vector<hpx::naming::id_type> workqueues;
  for (auto const& loc : hpx::find_all_localities()) {
    workqueues.push_back(hpx::new_<workstealing::workqueue>(loc).get());
  }
  hpx::wait_all(hpx::lcos::broadcast<startScheduler_action>(hpx::find_all_localities(), workqueues));

  std::vector<std::uint64_t> cntMap;
  cntMap.resize(maxDepth);
  for (auto i = 1; i <= maxDepth; ++i) {
    cntMap[i] = 0;
  }
  cntMap[0] = 1; // Count root node

  expand<Space, Sol, Gen, ChildTask>(spawnDepth, maxDepth, 1, root, cntMap);

  hpx::wait_all(hpx::lcos::broadcast<stopScheduler_action>(hpx::find_all_localities()));

  // Add the count of the "main" thread (since this doesn't return the same way the other tasks do)
  auto reg = skeletons::Enum::Components::Registry<Space, Sol>::gReg;
  reg->updateCounts(cntMap);

  // Gather the counts
  std::vector<std::vector<uint64_t> > cntList;
  cntList = hpx::lcos::broadcast(enum_getCounts_act(), hpx::find_all_localities(), Space(), root).get();
  std::vector<uint64_t> res;
  res.resize(maxDepth + 1);
  for (auto i = 0; i <= maxDepth; ++i) {
    std::uint64_t totalCnt = 0;
    for (const auto & cnt : cntList) {
      totalCnt += cnt[i];
    }
    res[i] = totalCnt;
  }

  return res;
}

template <typename Space,
          typename Sol,
          typename Gen,
          typename ChildTask>
void
searchChildTask(unsigned spawnDepth,
                const unsigned maxDepth,
                unsigned depth,
                Sol c,
                hpx::naming::id_type p) {
  std::vector<std::uint64_t> cntMap;
  cntMap.resize(maxDepth + 1);
  for (auto i = 0; i <= maxDepth; ++i) {
    cntMap[i] = 0;
  }

  expand<Space, Sol, Gen, ChildTask>(spawnDepth, maxDepth, depth, c, cntMap);

  // Atomically updates the (process) local counter
  auto reg = skeletons::Enum::Components::Registry<Space, Sol>::gReg;
  reg->updateCounts(cntMap);

  workstealing::tasks_required_sem.signal();
  hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(p, true).get();
}

}}}

#endif
