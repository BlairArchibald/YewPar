#ifndef SKELETONS_ENUM_DIST_NODEGENSTACK_HPP
#define SKELETONS_ENUM_DIST_NODEGENSTACK_HPP

#include <vector>
#include <array>
#include <stack>
#include <cstdint>
#include <chrono>
#include <unordered_map>
#include <memory>

#include <hpx/lcos/broadcast.hpp>
#include <hpx/runtime/get_ptr.hpp>
#include <hpx/include/iostreams.hpp>

#include "enumRegistry.hpp"
#include "util.hpp"

#include "util/func.hpp"
#include "util/util.hpp"

#include "workstealing/policies/SearchManager.hpp"
#include "workstealing/Scheduler.hpp"

#include "seq.hpp"

namespace skeletons { namespace Enum { namespace Debug {
std::unordered_map<std::size_t, std::vector<std::uint64_t> > taskTimes;

void printTaskTimes () {
  auto numThreads = hpx::get_os_thread_count();
  for (std::size_t t = 0; t < numThreads; ++t) {
    const auto taskList = taskTimes[t];
    for (const auto & cnt : taskList) {
      // TODO: We can even put the thread in here
      std::cout
          << (boost::format("Task ran on locality: %1% for: %2% us") % static_cast<std::int64_t>(hpx::get_locality_id()) % cnt)
          << std::endl;
    }
  }
}
HPX_DEFINE_PLAIN_ACTION(printTaskTimes, printTaskTimes_act);

}}}

HPX_REGISTER_ACTION(skeletons::Enum::Debug::printTaskTimes_act, skeletonEnumDebugPrintTaskTimes_act);

// Represent the search tree as a stealable stack of nodegens
namespace skeletons { namespace Enum {

struct StackOfNodes {};

// Default to no debug information
template <typename Space, typename Sol, typename Gen, std::size_t maxStackDepth_>
struct DistCount<Space, Sol, Gen, StackOfNodes, std::integral_constant<std::size_t, maxStackDepth_> > :
      DistCount<Space, Sol, Gen, StackOfNodes, std::integral_constant<std::size_t, maxStackDepth_>, std::integral_constant<bool, false> > {};

template <typename Space, typename Sol, typename Gen, std::size_t maxStackDepth_, bool Debug>
struct DistCount<Space, Sol, Gen, StackOfNodes, std::integral_constant<std::size_t, maxStackDepth_>, std::integral_constant<bool, Debug> > {

  struct StackElem {
    unsigned seen;
    typename Gen::return_type generator;
  };

  static void runFromSol(const Sol initNode,
                         const unsigned depth,
                         const hpx::naming::id_type donePromise) {
    auto reg = Registry<Space, Sol>::gReg;

    std::array<StackElem, maxStackDepth_> generatorStack;
    std::vector<std::uint64_t> cntMap(reg->maxDepth + 1);

    // Setup the stack with root node
    auto rootGen = Gen::invoke(reg->space, initNode);
    cntMap[depth] += rootGen.numChildren;
    generatorStack[0].seen = 0;
    generatorStack[0].generator = std::move(rootGen);

    // Register with the Policy to allow stealing from this stack
    std::shared_ptr<SharedState_t> stealReq;
    unsigned threadId;
    std::tie(stealReq, threadId) = std::static_pointer_cast<Policy_t>(Workstealing::Scheduler::local_policy)->registerThread();

    runTaskFromStack(depth, reg->maxDepth, reg->space, generatorStack, stealReq, cntMap, donePromise, threadId);
  }

  using ChildTask = func<
    decltype(&DistCount<Space, Sol, Gen, StackOfNodes, std::integral_constant<std::size_t, maxStackDepth_>, std::integral_constant<bool, Debug> >::runFromSol),
    &DistCount<Space, Sol, Gen, StackOfNodes, std::integral_constant<std::size_t, maxStackDepth_>, std::integral_constant<bool, Debug> >::runFromSol >;

  using Policy_t      = Workstealing::Policies::SearchManager<Sol, ChildTask>;
  using Response_t    = typename Policy_t::Response_t;
  using SharedState_t = typename Policy_t::SharedState_t;

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
          // Work left at this level:
          if (generatorStack[i].seen < generatorStack[i].generator.numChildren) {
            // Steal it all
            if (std::get<2>(*stealRequest)) {
              Response_t res;
              while (generatorStack[i].seen < generatorStack[i].generator.numChildren) {
                generatorStack[i].seen++;

                promises.emplace_back();
                auto & prom = promises.back();

                futures.push_back(prom.get_future());

                const auto stolenSol = generatorStack[i].generator.next();
                res.emplace_back(hpx::util::make_tuple(stolenSol, startingDepth + i + 1, prom.get_id()));
              }
              std::get<1>(*stealRequest).set(res);
              responded = true;
              break;
              // Steal the first task
            } else {
              generatorStack[i].seen++;

              promises.emplace_back();
              auto & prom = promises.back();

              futures.push_back(prom.get_future());

              const auto stolenSol = generatorStack[i].generator.next();
              Response_t res {hpx::util::make_tuple(stolenSol, startingDepth + i + 1, prom.get_id())};
              std::get<1>(*stealRequest).set(res);

              responded = true;
              break;
            }
          }
        }
        if (!responded) {
          Response_t res;
          std::get<1>(*stealRequest).set(res);
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
                               std::vector<hpx::future<void> > & futures){
    // Make sure the master locality is at the back of all other localities
    // since it takes the rest of the spawned work
    auto localities = util::findOtherLocalities();
    localities.push_back(hpx::find_here());

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

          // This needs to go to localities no managers now
          auto mgr = tasksSpawned % localities.size();
          hpx::async<addWorkAct>(localities[mgr], child, depth, pid);

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
                      const Sol & root) {
    auto reg = Registry<Space, Sol>::gReg;

    std::vector<hpx::future<void> > futures;
    std::vector<std::uint64_t> cntMap(maxDepth + 1);

    // Push work
    // FIXME: Assumes homogeneous machines
    auto totalThreads = hpx::find_all_localities().size() * (hpx::get_os_thread_count() - 1);

    if (totalThreads == 0) {
      hpx::promise<void> prom;
      auto f = prom.get_future();
      auto pid = prom.get_id();

      hpx::async<addWorkAct>(hpx::find_here(), root, 1, pid);

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

    auto stackDepth = 0;
    auto depth = 1;
    spawnInitialWork(depthRequired, totalThreads, stackDepth, depth, space, generatorStack, cntMap, futures);

    // Register the rest of the work from the main thread with the generator
    auto searchMgrInfo = std::static_pointer_cast<Policy_t>(Workstealing::Scheduler::local_policy)->registerThread();
    auto stealRequest  = std::get<0>(searchMgrInfo);

    // Continue the actual work
    hpx::promise<void> prom;
    auto f = prom.get_future();
    auto pid = prom.get_id();

    // Launch initialising thread as a new Scheduler
    hpx::threads::executors::default_executor exe(hpx::threads::thread_priority_critical,
                                                  hpx::threads::thread_stacksize_huge);
    hpx::apply(exe, &Workstealing::Scheduler::scheduler,
               hpx::util::bind(&runTaskFromStack, 1, maxDepth, space, generatorStack, stealRequest, cntMap, pid, std::get<1>(searchMgrInfo), stackDepth, depth));

    futures.push_back(std::move(f));
    hpx::wait_all(futures);
  }

  static std::vector<std::uint64_t> count(const unsigned maxDepth,
                                          const Space & space,
                                          const Sol   & root,
                                          bool stealAll = false) {
    hpx::wait_all(hpx::lcos::broadcast<EnumInitRegistryAct<Space, Sol> >(hpx::find_all_localities(), space, maxDepth, root));

    Policy_t::initPolicy(stealAll);

    doCount(maxDepth, space, root);

    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::stopSchedulers_act>(hpx::find_all_localities()));

    if (Debug) {
      for (auto const& loc : hpx::find_all_localities()) {
        hpx::async<Debug::printTaskTimes_act>(loc).get();
      }
      for (auto const& loc : hpx::find_all_localities()) {
        hpx::async<Workstealing::Policies::SearchManagerPerf::printDistributedStealsList_act>(loc).get();
      }
    }

    return totalNodeCounts<Space, Sol>(maxDepth);
  }

  static void runTaskFromStack (const unsigned startingDepth,
                                const unsigned maxDepth,
                                const Space & space,
                                std::array<StackElem, maxStackDepth_> & generatorStack,
                                const std::shared_ptr<SharedState_t> stealRequest,
                                std::vector<std::uint64_t> & cntMap,
                                const hpx::naming::id_type donePromise,
                                const unsigned searchManagerId,
                                const int stackDepth = 0,
                                const int depth = -1) {

    std::chrono::time_point<std::chrono::steady_clock> start_time;
    if (Debug) {
      start_time = std::chrono::steady_clock::now();
    }

    auto reg = Registry<Space, Sol>::gReg;
    std::vector<hpx::future<void> > futures;

    runWithStack(startingDepth, maxDepth, space, generatorStack, stealRequest, cntMap, futures, stackDepth, depth);

    // Atomically updates the (process) local counter
    reg->updateCounts(cntMap);

    std::static_pointer_cast<Policy_t>(Workstealing::Scheduler::local_policy)->unregisterThread(searchManagerId);

    if (Debug) {
      auto overall_time = std::chrono::duration_cast<std::chrono::microseconds>
                          (std::chrono::steady_clock::now() - start_time);
      auto thread = hpx::get_worker_thread_num();
      Debug::taskTimes[thread].emplace_back(overall_time.count());
    }

    hpx::apply(hpx::util::bind([=](std::vector<hpx::future<void> > & futs) {
          hpx::wait_all(futs);
          hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(donePromise, true);
        }, std::move(futures)));
  }

  // Action to push a new scheduler running this skeleton to a distributed node
  // (for setting initial work distribution)
  static void addWork (const Sol initNode,
                       const unsigned depth,
                       const hpx::naming::id_type donePromise) {
    hpx::threads::executors::default_executor exe(hpx::threads::thread_priority_critical,
                                                  hpx::threads::thread_stacksize_huge);
    hpx::apply(exe, &Workstealing::Scheduler::scheduler, hpx::util::bind(ChildTask::fn_ptr(), initNode, depth, donePromise));
  }
  struct addWorkAct : hpx::actions::make_action<
    decltype(&DistCount<Space, Sol, Gen, StackOfNodes, std::integral_constant<std::size_t, maxStackDepth_>, std::integral_constant<bool, Debug> >::addWork),
    &DistCount<Space, Sol, Gen, StackOfNodes, std::integral_constant<std::size_t, maxStackDepth_>, std::integral_constant<bool, Debug> >::addWork,
    addWorkAct>::type {};

};

}}

#endif
