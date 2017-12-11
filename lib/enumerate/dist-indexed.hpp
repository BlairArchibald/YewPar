#ifndef SKELETONS_ENUM_DIST_INDEXED_HPP
#define SKELETONS_ENUM_DIST_INDEXED_HPP

#include <hpx/lcos/broadcast.hpp>

#include <vector>
#include <cstdint>

#include "enumRegistry.hpp"
#include "util.hpp"

#include "util/util.hpp"
#include "util/func.hpp"
#include "util/positionIndex.hpp"

#include "workstealing/policies/SearchManager.hpp"
#include "workstealing/Scheduler.hpp"

namespace skeletons { namespace Enum {

template <typename Space, typename Sol, typename Gen>
struct DistCount<Space, Sol, Gen, Indexed> {

  static void searchChildTask(std::vector<unsigned> path,
                              const int depth,
                              const hpx::naming::id_type doneProm) {
    auto reg = Registry<Space, Sol>::gReg;

    auto posIdx = positionIndex(path);
    auto c = getStartingNode(path);

    std::vector<std::uint64_t> cntMap;
    cntMap.resize(reg->maxDepth + 1);

    std::shared_ptr<SharedState_t> stealRequest; unsigned idx;
    std::tie(stealRequest, idx) = std::static_pointer_cast<Policy_t>(Workstealing::Scheduler::local_policy)->registerThread();

    expand(posIdx, reg->maxDepth, depth, c, stealRequest, cntMap);

    std::static_pointer_cast<Policy_t>(Workstealing::Scheduler::local_policy)->unregisterThread(idx);

    // Atomically updates the (process) local counter
    reg->updateCounts(cntMap);

    hpx::apply([=](auto pIdx) {
          pIdx.waitFutures();
          hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(doneProm, true).get();
      }, std::move(posIdx));
  }

  using ChildTask = func<
    decltype(&DistCount<Space, Sol, Gen, Indexed>::searchChildTask),
    &DistCount<Space, Sol, Gen, Indexed>::searchChildTask>;

  using Policy_t      = Workstealing::Policies::SearchManager<std::vector<unsigned>, ChildTask>;
  using Response_t    = typename Policy_t::Response_t;
  using SharedState_t = typename Policy_t::SharedState_t;

  static void expand(positionIndex & pos,
                     const unsigned maxDepth,
                     unsigned depth,
                     const Sol & n,
                     std::shared_ptr<SharedState_t> stealRequest,
                     std::vector<std::uint64_t> & cntMap) {
    auto reg = Registry<Space, Sol>::gReg;

    // Handle Steal requests before processing a node
    if (std::get<0>(*stealRequest)) {
      auto stealAll = std::get<2>(*stealRequest);
      std::get<1>(*stealRequest).set(pos.steal(stealAll));
      std::get<0>(*stealRequest).store(false);
    }

    auto newCands = Gen::invoke(reg->space, n);
    pos.setNumChildren(newCands.numChildren);

    cntMap[depth] += newCands.numChildren;

    if (maxDepth == depth) {
      return;
    }

    auto i = 0;
    int nextPos;
    while ((nextPos = pos.getNextPosition()) >= 0) {
      auto c = newCands.next();

      if (nextPos != i) {
        for (auto j = 0; j < nextPos - i; ++j) {
          c = newCands.next();
        }
        i += nextPos - i;
      }

      pos.preExpand(i);
      expand(pos, maxDepth, depth + 1, c, stealRequest, cntMap);
      pos.postExpand();

      ++i;
    }
  }

  static std::vector<std::uint64_t> count(const unsigned maxDepth,
                                          const Space & space,
                                          const Sol   & root,
                                          const bool stealAll = false) {
    hpx::wait_all(hpx::lcos::broadcast<EnumInitRegistryAct<Space, Sol> >(hpx::find_all_localities(), space, maxDepth, root));

    Workstealing::Policies::SearchManager<std::vector<unsigned>, ChildTask>::initPolicy(stealAll);

    auto distributedThreads = hpx::get_os_thread_count() == 1 ? 1 : hpx::get_os_thread_count() - 1;
    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::startSchedulers_act>(util::findOtherLocalities(), distributedThreads));

    // Start N-2 local schedulers since we start another below
    auto localThreads = hpx::get_os_thread_count() < 3 ? 0 : hpx::get_os_thread_count() - 2;
    hpx::async<Workstealing::Scheduler::startSchedulers_act>(hpx::find_here(), localThreads).get();

    // Push the root node as a task to the searchManager
    std::vector<unsigned> path;
    path.reserve(30);
    path.push_back(0);
    hpx::promise<void> prom;
    auto f = prom.get_future();
    auto pid = prom.get_id();

    // Start off the main scheduler
    hpx::threads::executors::default_executor exe(hpx::threads::thread_priority_critical,
                                                  hpx::threads::thread_stacksize_huge);
    hpx::apply(exe, &Workstealing::Scheduler::scheduler, hpx::util::bind(&searchChildTask, path, 1, pid));

    // Wait completion of the main task
    f.get();

    // Stop the schedulers everywhere
    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::stopSchedulers_act>(hpx::find_all_localities()));

    // Gather the counts
    return totalNodeCounts<Space, Sol>(maxDepth);
  }

  static Sol getStartingNode(std::vector<unsigned> & path) {
    auto reg  = Registry<Space, Sol>::gReg;
    auto node = reg->root;

    // Paths have a leading 0 (representing root, we don't need this).
    path.erase(path.begin());

    if (path.empty()) {
      return node;
    }

    for (auto const & p : path) {
      auto newCands = Gen::invoke(reg->space, node);
      node = newCands.nth(p);
    }

    return node;
  }
};

}}

#endif
