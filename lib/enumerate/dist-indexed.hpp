#ifndef SKELETONS_ENUM_DIST_INDEXED_HPP
#define SKELETONS_ENUM_DIST_INDEXED_HPP

#include <hpx/lcos/broadcast.hpp>

#include <vector>
#include <cstdint>

#include "enumRegistry.hpp"

#include "util/positionIndex.hpp"

#include "workstealing/indexedScheduler.hpp"
#include "workstealing/posManager.hpp"

namespace skeletons { namespace Enum {

template <typename Space, typename Sol, typename Gen>
struct DistCount<Space, Sol, Gen, Indexed> {

  static void searchChildTask(const std::shared_ptr<positionIndex> posIdx,
                              const hpx::naming::id_type p,
                              const int idx,
                              const hpx::naming::id_type posMgr) {
    auto reg = skeletons::Enum::Components::Registry<Space, Sol>::gReg;

    auto path = posIdx->getPath();
    auto depth = path.size();
    auto c = getStartingNode(path);

    std::vector<std::uint64_t> cntMap;
    cntMap.resize(reg->maxDepth + 1);
    for (auto i = 0; i <= reg->maxDepth; ++i) {
      cntMap[i] = 0;
    }

    // Account for the root node (paths always include 0 so we need to subtract one to test if we are the root)
    if (depth - 1 == 0) {
      cntMap[0] = 1;
    }
    expand(*posIdx, reg->maxDepth, depth, c, cntMap);

    // Atomically updates the (process) local counter
    reg->updateCounts(cntMap);

    hpx::async<workstealing::indexed::posManager::done_action>(posMgr, idx).get();
    workstealing::indexed::tasks_required_sem.signal();

    posIdx->waitFutures();
    hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(p, true).get();
  }

  struct ChildTask : hpx::actions::make_action<
    decltype(&DistCount<Space, Sol, Gen, Indexed>::searchChildTask),
    &DistCount<Space, Sol, Gen, Indexed>::searchChildTask,
    ChildTask>::type {};

  static void expand(positionIndex & pos,
                     const unsigned maxDepth,
                     unsigned depth,
                     const Sol & n,
                     std::vector<std::uint64_t> & cntMap) {
    auto reg = Components::Registry<Space, Sol>::gReg;

    auto newCands = Gen::invoke(0, reg->space_, n);
    pos.setNumChildren(newCands.numChildren);

    cntMap[depth] += newCands.numChildren;

    if (maxDepth == depth) {
      return;
    }

    auto i = 0;
    int nextPos;
    while ((nextPos = pos.getNextPosition()) >= 0) {
      auto c = newCands.next(reg->space_, n);

      if (nextPos != i) {
        for (auto j = 0; j < nextPos - i; ++j) {
          c = newCands.next(reg->space_, n);
        }
        i += nextPos - i;
      }

      pos.preExpand(i);
      expand(pos, maxDepth, depth + 1, c, cntMap);
      pos.postExpand();

      ++i;
    }
  }

  static std::vector<std::uint64_t> count(const unsigned maxDepth,
                                          const Space & space,
                                          const Sol   & root) {
    hpx::wait_all(hpx::lcos::broadcast<enum_initRegistry_act>(hpx::find_all_localities(), space, maxDepth, root));

    auto posMgrs = hpx::lcos::broadcast<workstealing::indexed::initPosMgr_act<ChildTask> >(hpx::find_all_localities()).get();

    hpx::naming::id_type localPosMgr;
    for (auto it = posMgrs.begin(); it != posMgrs.end(); ++it) {
      if (hpx::get_colocation_id(*it).get() == hpx::find_here()) {
        localPosMgr = *it;
        break;
      }
    }

    hpx::async<startScheduler_indexed_action>(hpx::find_here(), posMgrs).get();

    // Push the root node as a task to the posMgr
    std::vector<unsigned> path;
    path.reserve(30);
    path.push_back(0);
    hpx::promise<void> prom;
    auto f = prom.get_future();
    auto pid = prom.get_id();
    hpx::async<workstealing::indexed::posManager::addWork_action>(localPosMgr, path, pid);

    // Wait completion of the main task
    f.get();

    // Stop the schedulers everywhere
    hpx::wait_all(hpx::lcos::broadcast<stopScheduler_indexed_action>(hpx::find_all_localities()));

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

  static Sol getStartingNode(std::vector<unsigned> & path) {
    auto reg  = skeletons::Enum::Components::Registry<Space, Sol>::gReg;
    auto node =  reg->root_;

    // Paths have a leading 0 (representing root, we don't need this).
    path.erase(path.begin());

    if (path.empty()) {
      return node;
    }

    for (auto const & p : path) {
      auto newCands = Gen::invoke(0, reg->space_, node);
      node = newCands.nth(reg->space_, node, p);
    }

    return node;
  }
};

}}

#endif
