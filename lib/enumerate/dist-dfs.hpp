#ifndef SKELETONS_ENUM_DIST_DFS_HPP
#define SKELETONS_ENUM_DIST_DFS_HPP

#include <vector>
#include <hpx/lcos/broadcast.hpp>
#include <cstdint>

#include "enumRegistry.hpp"
#include "util.hpp"

#include "workstealing/scheduler.hpp"
#include "workstealing/workqueue.hpp"

namespace skeletons { namespace Enum {

template <typename Space, typename Sol, typename Gen>
struct DistCount<Space, Sol, Gen> {

  static void searchChildTask(unsigned spawnDepth,
                              const unsigned maxDepth,
                              unsigned depth,
                              Sol c,
                              hpx::naming::id_type p) {
    std::vector<std::uint64_t> cntMap(maxDepth + 1);

    expand(spawnDepth, maxDepth, depth, c, cntMap);

    // Atomically updates the (process) local counter
    auto reg = Registry<Space, Sol>::gReg;
    reg->updateCounts(cntMap);

    workstealing::tasks_required_sem.signal();
    hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(p, true).get();
  }

  struct ChildTask : hpx::actions::make_action<
    decltype(&DistCount<Space, Sol, Gen>::searchChildTask),
    &DistCount<Space, Sol, Gen>::searchChildTask,
    ChildTask>::type {};

  static void expand(unsigned spawnDepth,
                     const unsigned maxDepth,
                     unsigned depth,
                     const Sol & n,
                     std::vector<std::uint64_t> & cntMap) {
    auto reg = Registry<Space, Sol>::gReg;

    auto newCands = Gen::invoke(reg->space_, n);

    std::vector<hpx::future<void> > childFuts;
    if (spawnDepth > 0) {
      childFuts.reserve(newCands.numChildren);
    }

    cntMap[depth] += newCands.numChildren;

    if (maxDepth == depth) {
      return;
    }

    for (auto i = 0; i < newCands.numChildren; ++i) {
      auto c = newCands.next();

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
        expand(0, maxDepth, depth + 1, c, cntMap);
      }
    }

    if (spawnDepth > 0) {
      workstealing::tasks_required_sem.signal(); // Going to sleep, get more tasks
      hpx::wait_all(childFuts);
    }
  }

  static std::vector<std::uint64_t> count(unsigned spawnDepth,
                                          const unsigned maxDepth,
                                          const Space & space,
                                          const Sol   & root) {

    hpx::wait_all(hpx::lcos::broadcast<EnumInitRegistryAct<Space, Sol> >(hpx::find_all_localities(), space, maxDepth, root));

    std::vector<hpx::naming::id_type> workqueues;
    for (auto const& loc : hpx::find_all_localities()) {
      workqueues.push_back(hpx::new_<workstealing::workqueue>(loc).get());
    }
    hpx::wait_all(hpx::lcos::broadcast<startScheduler_action>(hpx::find_all_localities(), workqueues));

    std::vector<std::uint64_t> cntMap(maxDepth + 1);

    expand(spawnDepth, maxDepth, 1, root, cntMap);

    hpx::wait_all(hpx::lcos::broadcast<stopScheduler_action>(hpx::find_all_localities()));

    // Add the count of the "main" thread (since this doesn't return the same way the other tasks do)
    auto reg = Registry<Space, Sol>::gReg;
    reg->updateCounts(cntMap);

    // Gather the counts
    return totalNodeCounts<Space, Sol>(maxDepth);
  }

};

}}

#endif
