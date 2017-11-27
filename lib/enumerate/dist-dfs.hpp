#ifndef SKELETONS_ENUM_DIST_DFS_HPP
#define SKELETONS_ENUM_DIST_DFS_HPP

#include <vector>
#include <hpx/lcos/broadcast.hpp>
#include <cstdint>

#include "enumRegistry.hpp"
#include "util.hpp"

#include "workstealing/Scheduler.hpp"
#include "workstealing/policies/Workpool.hpp"

namespace skeletons { namespace Enum {

template <typename Space, typename Sol, typename Gen>
struct DistCount<Space, Sol, Gen> {

  static void searchChildTask(unsigned spawnDepth,
                              const unsigned maxDepth,
                              unsigned depth,
                              Sol c,
                              hpx::naming::id_type p) {
    std::vector<std::uint64_t> cntMap(maxDepth + 1);
    std::vector<hpx::future<void> > childFutures;

    expand(spawnDepth, maxDepth, depth, c, cntMap, childFutures);

    // Atomically updates the (process) local counter
    auto reg = Registry<Space, Sol>::gReg;
    reg->updateCounts(cntMap);

    hpx::apply(hpx::util::bind([=](std::vector<hpx::future<void> > & futs) {
          hpx::wait_all(futs);
          hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(p, true);
        }, std::move(childFutures)));
  }

  struct ChildTask : hpx::actions::make_action<
    decltype(&DistCount<Space, Sol, Gen>::searchChildTask),
    &DistCount<Space, Sol, Gen>::searchChildTask,
    ChildTask>::type {};

  static hpx::future<void> createTask(const unsigned spawnDepth,
                                      const unsigned maxDepth,
                                      const unsigned depth,
                                      const Sol & c) {
    hpx::lcos::promise<void> prom;
    auto pfut = prom.get_future();
    auto pid  = prom.get_id();

    ChildTask t;
    hpx::util::function<void(hpx::naming::id_type)> task;
    task = hpx::util::bind(t, hpx::util::placeholders::_1, spawnDepth, maxDepth, depth, c, pid);
    std::static_pointer_cast<Workstealing::Policies::Workpool>(Workstealing::Scheduler::local_policy)->addwork(task);

    return pfut;
  }

  static void expand(const unsigned spawnDepth,
                     const unsigned maxDepth,
                     const unsigned depth,
                     const Sol & n,
                     std::vector<std::uint64_t> & cntMap,
                     std::vector<hpx::future<void> > & childFutures) {
    auto reg = Registry<Space, Sol>::gReg;

    auto newCands = Gen::invoke(reg->space, n);
    cntMap[depth] += newCands.numChildren;

    if (maxDepth == depth) {
      return;
    }

    for (auto i = 0; i < newCands.numChildren; ++i) {
      auto c = newCands.next();

      /* Search the child nodes */
      if (spawnDepth > 0) {
        auto pfut = createTask(spawnDepth - 1, maxDepth, depth + 1, c);
        childFutures.push_back(std::move(pfut));
      } else {
        expandNoSpawns(maxDepth, depth + 1, reg->space, c, cntMap);
      }
    }
  }

  // Optimised version of expand, for performance
  static void expandNoSpawns(const unsigned maxDepth,
                             const unsigned depth,
                             const Space & space,
                             const Sol & n,
                             std::vector<std::uint64_t> & cntMap
                             ) {
    auto newCands = Gen::invoke(space, n);
    cntMap[depth] += newCands.numChildren;

    if (maxDepth == depth) {
      return;
    }

    for (auto i = 0; i < newCands.numChildren; ++i) {
      auto c = newCands.next();
      expandNoSpawns(maxDepth, depth + 1, space, c, cntMap);
    }
  }

  static std::vector<std::uint64_t> count(unsigned spawnDepth,
                                          const unsigned maxDepth,
                                          const Space & space,
                                          const Sol   & root) {

    hpx::wait_all(hpx::lcos::broadcast<EnumInitRegistryAct<Space, Sol> >(hpx::find_all_localities(), space, maxDepth, root));

    Workstealing::Policies::Workpool::initPolicy();

    auto threadCount = hpx::get_os_thread_count() == 1 ? 1 : hpx::get_os_thread_count() - 1;
    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::startSchedulers_act>(hpx::find_all_localities(), threadCount));

    // Create the main task and wait for it to finish
    createTask(spawnDepth, maxDepth, 1, root).get();

    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::stopSchedulers_act>(hpx::find_all_localities()));

    // Gather the counts
    return totalNodeCounts<Space, Sol>(maxDepth);
  }

};

}}

#endif
