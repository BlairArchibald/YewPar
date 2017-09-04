#ifndef SKELETONS_ENUM_DIST_NODEGENSTACK_HPP
#define SKELETONS_ENUM_DIST_NODEGENSTACK_HPP

#include <vector>
#include <array>
#include <stack>
#include <cstdint>

#include <hpx/lcos/broadcast.hpp>

#include "enumRegistry.hpp"
#include "util/NodeStack.hpp"
#include "util/func.hpp"

#include "workstealing/NodeStackManager.hpp"
#include "workstealing/NodeStackScheduler.hpp"

// Represent the search tree as a stealable stack of nodegens
namespace skeletons { namespace Enum {

struct StackOfNodes {};

template <typename Space, typename Sol, typename Gen, std::size_t maxStackDepth_>
struct DistCount<Space, Sol, Gen, StackOfNodes, std::integral_constant<std::size_t, maxStackDepth_> > {

  using StackType = NodeStack<Sol, typename Gen::return_type, maxStackDepth_>;

  static void expandFromDepth(StackType & stack,
                              const unsigned startingDepth,
                              const unsigned maxDepth,
                              const Space & space,
                              const Sol & s,
                              std::vector<std::uint64_t> & cntMap) {
    auto gen = Gen::invoke(space, s);
    auto depth = startingDepth;

    stack.push(std::move(gen));
    cntMap[depth] += gen.numChildren;

    // Don't enter the loop unless there is work to do
    if (maxDepth == 1) {
      return;
    }

    while (!stack.empty()) {
      auto & curGen = stack.getCurrentFrame();
      if(std::get<0>(curGen) == 0) {
        stack.pop();
        depth--;
        continue;
      }

      // Move to the child
      auto child = std::get<1>(curGen).next();
      std::get<0>(curGen)--;

      depth++;

      auto gen = Gen::invoke(space, child);
      cntMap[depth] += gen.numChildren;

      if (maxDepth == depth) {
        depth--;
        continue;
      }

      stack.push(std::move(gen));
    }
  }

  static std::vector<std::uint64_t> count(const unsigned maxDepth,
                                          const Space & space,
                                          const Sol   & root) {
    //FIXME: Sequential version for now
    hpx::wait_all(hpx::lcos::broadcast<enum_initRegistry_act>(hpx::find_all_localities(), space, maxDepth, root));

    hpx::naming::id_type localMgr;
    std::vector<hpx::naming::id_type> mgrs;
    for (auto const& loc : hpx::find_all_localities()) {
      auto mgr = hpx::new_<workstealing::NodeStackManager<Sol, typename Gen::return_type, maxStackDepth_, ChildTask> >(loc).get();
      mgrs.push_back(mgr);
      if (loc == hpx::find_here()) {
        localMgr = mgr;
      }
    }

    typedef typename workstealing::NodeStackSched::startSchedulerAct<Sol, typename Gen::return_type, maxStackDepth_, ChildTask> startSchedulerAct;
    hpx::wait_all(hpx::lcos::broadcast<startSchedulerAct>(hpx::find_all_localities(), mgrs));

    // Add the root task as some work
    hpx::promise<void> prom;
    auto f = prom.get_future();
    auto pid = prom.get_id();

    typedef typename workstealing::NodeStackManager<Sol, typename Gen::return_type, maxStackDepth_, ChildTask>::addWork_action addWorkAct;
    hpx::async<addWorkAct>(localMgr, root, pid);

    f.get();

    hpx::wait_all(hpx::lcos::broadcast<stopScheduler_NodeStack_action>(hpx::find_all_localities()));

    std::vector<std::vector<uint64_t> > cntList;
    cntList = hpx::lcos::broadcast(enum_getCounts_act(), hpx::find_all_localities(), Space(), root).get();
    std::vector<uint64_t> res(maxDepth + 1);
    for (auto i = 0; i <= maxDepth; ++i) {
      std::uint64_t totalCnt = 0;
      for (const auto & cnt : cntList) {
        totalCnt += cnt[i];
      }
      res[i] = totalCnt;
    }
    res[0] = 1; //Account for root node
    return res;
  }

  static void runFromSol(const Sol & initNode,
                         const unsigned depth,
                         const std::shared_ptr<StackType> stack,
                         const hpx::naming::id_type p,
                         const int idx,
                         const hpx::naming::id_type NodeStackMgr) {
    auto reg = skeletons::Enum::Components::Registry<Space, Sol>::gReg;

    std::vector<std::uint64_t> cntMap(reg->maxDepth + 1);

    expandFromDepth(*stack, depth, reg->maxDepth, reg->space_, initNode, cntMap);

    // Atomically updates the (process) local counter
    reg->updateCounts(cntMap);

    typedef typename workstealing::NodeStackManager<Sol, typename Gen::return_type, maxStackDepth_, ChildTask>::done_action doneAct;
    hpx::async<doneAct>(NodeStackMgr, idx).get();
    workstealing::tasks_required_sem.signal();

    stack->waitFutures();
    hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(p, true).get();
  }

  using ChildTask = func<
    decltype(&DistCount<Space, Sol, Gen, StackOfNodes, std::integral_constant<std::size_t, maxStackDepth_> >::runFromSol),
    &DistCount<Space, Sol, Gen, StackOfNodes, std::integral_constant<std::size_t, maxStackDepth_> >::runFromSol >;
};



}}

#endif
