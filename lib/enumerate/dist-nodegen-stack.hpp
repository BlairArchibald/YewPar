#ifndef SKELETONS_ENUM_DIST_NODEGENSTACK_HPP
#define SKELETONS_ENUM_DIST_NODEGENSTACK_HPP

#include <vector>
#include <array>
#include <stack>
#include <cstdint>

#include <boost/optional.hpp>

#include <hpx/lcos/broadcast.hpp>

#include "enumRegistry.hpp"
#include "util/func.hpp"

#include "workstealing/SearchManager.hpp"
#include "workstealing/SearchManagerScheduler.hpp"

// Represent the search tree as a stealable stack of nodegens
namespace skeletons { namespace Enum {

struct StackOfNodes {};

template <typename Space, typename Sol, typename Gen, std::size_t maxStackDepth_>
struct DistCount<Space, Sol, Gen, StackOfNodes, std::integral_constant<std::size_t, maxStackDepth_> > {

  struct StackElem {
    unsigned seen;
    typename Gen::return_type generator;
  };

  using Response_t    = boost::optional<hpx::util::tuple<Sol, int, hpx::naming::id_type> >;
  using SharedState_t = std::pair<std::atomic<bool>, hpx::lcos::local::one_element_channel<Response_t> >;

  static void expandFromDepth(const int startingDepth,
                              const unsigned maxDepth,
                              const Space & space,
                              const Sol & startingNode,
                              std::shared_ptr<SharedState_t> stealRequest,
                              std::vector<std::uint64_t> & cntMap,
                              std::vector<hpx::future<void> > & futures) {
    std::array<StackElem, maxStackDepth_> generatorStack;
    std::vector<hpx::promise<void> > promises;

    // Setup the stack with root node
    auto rootGen = Gen::invoke(space, startingNode);
    cntMap[startingDepth] += rootGen.numChildren;
    generatorStack[0].seen = 0;
    generatorStack[0].generator = std::move(rootGen);

    unsigned depth = startingDepth;
    int stackDepth = 0;
    while (stackDepth >= 0) {
      // Handle steals first
      if (std::get<0>(*stealRequest)) {

        // We steal from the highest possible generator with work
        bool responded = false;
        for (auto i = 0; i < stackDepth; ++i) {
          if (generatorStack[i].seen < generatorStack[i].generator.numChildren) {
            generatorStack[i].seen++;

            promises.emplace_back();
            auto & prom = promises.back();

            futures.push_back(prom.get_future());

            const auto stolenSol = generatorStack[i].generator.next();
            std::get<1>(*stealRequest).set(hpx::util::make_tuple(stolenSol, startingDepth + i + 1, prom.get_id()));

            responded = true;
            break;
          }
        }
        if (!responded) {
          std::get<1>(*stealRequest).set({});
        }
        std::get<0>(*stealRequest).store(false);
      }

      // If there's still children at this stackDepth we move into them
      if (generatorStack[stackDepth].seen < generatorStack[stackDepth].generator.numChildren) {
        // Get the next child at this stackDepth
        const auto child = generatorStack[stackDepth].generator.next();
        generatorStack[stackDepth].seen++;

        // Going down
        stackDepth++;
        depth++;

        // Get the child's generator
        const auto childGen = Gen::invoke(space, child);
        cntMap[depth] += childGen.numChildren;

        // We don't even need to store the generator if we are just counting the children
        if (depth == maxDepth) {
          stackDepth--;
          depth--;
        } else {
          generatorStack[stackDepth].seen = 0;
          generatorStack[stackDepth].generator = std::move(childGen);
        }
      } else {
        stackDepth--;
        depth--;
      }
    }
  }

  static std::vector<std::uint64_t> count(const unsigned maxDepth,
                                          const Space & space,
                                          const Sol   & root) {
    hpx::wait_all(hpx::lcos::broadcast<enum_initRegistry_act>(hpx::find_all_localities(), space, maxDepth, root));

    hpx::naming::id_type localSearchManager;
    std::vector<hpx::naming::id_type> searchManagers;
    for (auto const& loc : hpx::find_all_localities()) {
      auto mgr = hpx::new_<workstealing::SearchManager<Sol, ChildTask> >(loc).get();
      searchManagers.push_back(mgr);
      if (loc == hpx::find_here()) {
        localSearchManager = mgr;
      }
    }

    typedef typename workstealing::SearchManagerSched::startSchedulerAct<Sol, ChildTask> startSchedulerAct;
    hpx::wait_all(hpx::lcos::broadcast<startSchedulerAct>(hpx::find_all_localities(), searchManagers));

    // Add the root task as some work
    hpx::promise<void> prom;
    auto f = prom.get_future();
    auto pid = prom.get_id();

    typedef typename workstealing::SearchManager<Sol, ChildTask>::addWork_action addWorkAct;
    hpx::async<addWorkAct>(localSearchManager, root, 1, pid);

    f.get();

    hpx::wait_all(hpx::lcos::broadcast<stopScheduler_SearchManager_action>(hpx::find_all_localities()));

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

  static void runFromSol(const Sol initNode,
                         const unsigned depth,
                         const std::shared_ptr<SharedState_t> stealRequest,
                         const hpx::naming::id_type p,
                         const unsigned idx,
                         const hpx::naming::id_type searchManager) {
    auto reg = skeletons::Enum::Components::Registry<Space, Sol>::gReg;

    std::vector<std::uint64_t> cntMap(reg->maxDepth + 1);
    std::vector<hpx::future<void> > futures;

    expandFromDepth(depth, reg->maxDepth, reg->space_, initNode, stealRequest, cntMap, futures);

    // Atomically updates the (process) local counter
    reg->updateCounts(cntMap);

    typedef typename workstealing::SearchManager<Sol, ChildTask>::done_action doneAct;
    hpx::async<doneAct>(searchManager, idx).get();

    workstealing::SearchManagerSched::tasks_required_sem.signal();
    hpx::wait_all(futures);

    hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(p, true).get();
  }

  using ChildTask = func<
    decltype(&DistCount<Space, Sol, Gen, StackOfNodes, std::integral_constant<std::size_t, maxStackDepth_> >::runFromSol),
    &DistCount<Space, Sol, Gen, StackOfNodes, std::integral_constant<std::size_t, maxStackDepth_> >::runFromSol >;
};

}}

#endif
