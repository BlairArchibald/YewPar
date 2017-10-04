#ifndef SKELETONS_ENUM_DIST_NODEGENSTACK_HPP
#define SKELETONS_ENUM_DIST_NODEGENSTACK_HPP

#include <vector>
#include <array>
#include <stack>
#include <cstdint>

#include <boost/optional.hpp>

#include <hpx/lcos/broadcast.hpp>
#include <hpx/runtime/get_ptr.hpp>

#include "enumRegistry.hpp"
#include "util/func.hpp"

#include "workstealing/SearchManager.hpp"
#include "workstealing/SearchManagerScheduler.hpp"

#include "seq.hpp"

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

  // Run with a pre-initialised stack. Used for the master thread once it pushes work
  static void runWithStack(const int startingDepth,
                           const unsigned maxDepth,
                           const Space & space,
                           std::array<StackElem, maxStackDepth_> & generatorStack,
                           std::shared_ptr<SharedState_t> stealRequest,
                           std::vector<std::uint64_t> & cntMap,
                           std::vector<hpx::future<void> > & futures,
                           int stackDepth = 0,
                           int depth = -1) {
    std::vector<hpx::promise<void> > promises;

    // We do this because arguments can't default initialise to themselves
    if (depth == -1) {
      depth = startingDepth;
    }

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

  static unsigned getCountAt(const Space & space, const Sol & root, unsigned depth) {
    // Call the sequential skeleton to get a node count for the top levels
    auto cntMap = Count<Space, Sol, Gen>::search(depth, space, root);
    return cntMap[depth];
  }

  static void spawnInitialWork(unsigned depthRequired,
                               unsigned totalThreads,
                               int & stackDepth,
                               int & depth,
                               const Space & space,
                               std::array<StackElem, maxStackDepth_> & generatorStack,
                               std::vector<std::uint64_t> & cntMap,
                               std::vector<hpx::future<void> > & futures,
                               std::vector<hpx::naming::id_type> & searchMgrs){
    auto tasksSpawned = 0;

    while (stackDepth >= 0) {
      // If there's still children at this stackDepth we move into them
      if (generatorStack[stackDepth].seen < generatorStack[stackDepth].generator.numChildren) {
        // Get the next child at this stackDepth
        const auto child = generatorStack[stackDepth].generator.next();
        generatorStack[stackDepth].seen++;

        // Going down
        stackDepth++;
        depth++;

        // Push anything at this depth as a task
        if (stackDepth == depthRequired) {
          hpx::promise<void> prom;
          auto f = prom.get_future();
          auto pid = prom.get_id();
          futures.push_back(std::move(f));

          auto mgr = tasksSpawned % searchMgrs.size();
          typedef typename workstealing::SearchManager<Sol, ChildTask>::addWork_action addWorkAct;
          hpx::async<addWorkAct>(searchMgrs[mgr], child, depth, pid);

          stackDepth--;
          depth--;
          ++tasksSpawned;

          // We keep a spare thread for ourselves to execute as
          if (tasksSpawned == totalThreads - 1) {
            break;
          }
        } else {
          // Get the child's generator
          const auto childGen = Gen::invoke(space, child);
          cntMap[depth] += childGen.numChildren;
          generatorStack[stackDepth].seen = 0;
          generatorStack[stackDepth].generator = std::move(childGen);
        }
      } else {
        stackDepth--;
        depth--;
      }
    }
  }

  // Push a node to each thread in the system
  static void doCount(const unsigned maxDepth,
                      const Space & space,
                      const Sol & root,
                      std::vector<hpx::naming::id_type> & searchMgrs) {
    auto reg = skeletons::Enum::Components::Registry<Space, Sol>::gReg;

    std::vector<hpx::future<void> > futures;
    std::vector<std::uint64_t> cntMap(maxDepth + 1);

    // Push work
    // FIXME: Assumes homogeneous machines
    auto totalThreads = hpx::find_all_localities().size() * hpx::get_os_thread_count();

    // Assumes we always have a local manager in the list
    auto localSearchMgr = *(std::find_if(searchMgrs.begin(), searchMgrs.end(), [](const auto & id){
          return hpx::get_colocation_id(hpx::launch::sync, id) == hpx::find_here();
        }));

    if (totalThreads == 1) {
      hpx::promise<void> prom;
      auto f = prom.get_future();
      auto pid = prom.get_id();

      typedef typename workstealing::SearchManager<Sol, ChildTask>::addWork_action addWorkAct;
      hpx::async<addWorkAct>(localSearchMgr, root, 1, pid);

      f.get();

      return;
    }

    // Find the required depth to get "totalThreads" tasks
    // Just runs the top level counts multiple times for ease
    auto depthRequired = 1;
    while (depthRequired <= maxDepth) {
      auto numNodes = getCountAt(space, root, depthRequired);
      if (numNodes >= totalThreads) {
        break;
      }
      ++depthRequired;
    }

    // Potulate initial stack
    std::array<StackElem, maxStackDepth_> generatorStack;
    auto rootGen = Gen::invoke(space, root);
    cntMap[1] += rootGen.numChildren;
    generatorStack[0].seen = 0;
    generatorStack[0].generator = std::move(rootGen);

    // Local searchManager should be at the back of the searchMgrs since it only spawns threads - 1
    searchMgrs.erase(std::remove(searchMgrs.begin(), searchMgrs.end(), localSearchMgr), searchMgrs.end());
    searchMgrs.push_back(localSearchMgr);
    assert(localSearchMgr == searchMgrs.back());

    auto stackDepth = 0;
    auto depth = 1;
    spawnInitialWork(depthRequired, totalThreads, stackDepth, depth, space, generatorStack, cntMap, futures, searchMgrs);

    // Register the rest of the work from the main thread with the generator
    auto localSearchManagerPtr = hpx::get_ptr<workstealing::SearchManager<Sol, ChildTask> >(hpx::launch::sync, localSearchMgr);
    auto searchMgrInfo = localSearchManagerPtr->registerThread();
    auto stealRequest  = std::get<0>(searchMgrInfo);

    // Continue the actual work
    hpx::promise<void> prom;
    auto f = prom.get_future();
    auto pid = prom.get_id();

    hpx::threads::executors::default_executor exe(hpx::threads::thread_stacksize_large);
    hpx::apply(exe, hpx::util::bind(&runTaskFromStack, 1, maxDepth, space, generatorStack, stealRequest,
                                    cntMap, pid, std::get<1>(searchMgrInfo), localSearchMgr, stackDepth, depth));

    futures.push_back(std::move(f));
    hpx::wait_all(futures);
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

    doCount(maxDepth, space, root, searchManagers);

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

    std::array<StackElem, maxStackDepth_> generatorStack;
    std::vector<std::uint64_t> cntMap(reg->maxDepth + 1);

    // Setup the stack with root node
    auto rootGen = Gen::invoke(reg->space_, initNode);
    cntMap[depth] += rootGen.numChildren;
    generatorStack[0].seen = 0;
    generatorStack[0].generator = std::move(rootGen);

    runTaskFromStack(depth, reg->maxDepth, reg->space_, generatorStack, stealRequest, cntMap, p, idx, searchManager);
  }

  static void runTaskFromStack (const unsigned startingDepth,
                                const unsigned maxDepth,
                                const Space & space,
                                std::array<StackElem, maxStackDepth_> & generatorStack,
                                const std::shared_ptr<SharedState_t> stealRequest,
                                std::vector<std::uint64_t> & cntMap,
                                const hpx::naming::id_type donePromise,
                                const unsigned searchManagerId,
                                const hpx::naming::id_type searchManager,
                                const int stackDepth = 0,
                                const int depth = -1) {
    auto reg = skeletons::Enum::Components::Registry<Space, Sol>::gReg;
    std::vector<hpx::future<void> > futures;

    runWithStack(startingDepth, maxDepth, space, generatorStack, stealRequest, cntMap, futures, stackDepth, depth);

    // Atomically updates the (process) local counter
    reg->updateCounts(cntMap);

    typedef typename workstealing::SearchManager<Sol, ChildTask>::done_action doneAct;
    hpx::async<doneAct>(searchManager, searchManagerId).get();

    workstealing::SearchManagerSched::tasks_required_sem.signal();
    hpx::wait_all(futures);

    hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(donePromise, true).get();
  }

  using ChildTask = func<
    decltype(&DistCount<Space, Sol, Gen, StackOfNodes, std::integral_constant<std::size_t, maxStackDepth_> >::runFromSol),
    &DistCount<Space, Sol, Gen, StackOfNodes, std::integral_constant<std::size_t, maxStackDepth_> >::runFromSol >;
};

}}

#endif
