#ifndef SKELETONS_DEPTHSPAWN_HPP
#define SKELETONS_DEPTHSPAWN_HPP

#include <chrono>
#include <iostream>
#include <mutex>
#include <vector>
#include <cstdint>

#include "API.hpp"

#include <hpx/lcos/broadcast.hpp>
#include <hpx/include/iostreams.hpp>
#include <hpx/lcos/local/spinlock.hpp>

#include "util/NodeGenerator.hpp"
#include "util/Registry.hpp"
#include "util/Incumbent.hpp"
#include "util/func.hpp"
#include "util/MetricStore.hpp"

#include "Common.hpp"

#include "workstealing/Scheduler.hpp"
#include "workstealing/policies/Workpool.hpp"
#include "workstealing/policies/DepthPoolPolicy.hpp"

namespace YewPar { namespace Skeletons {

namespace DepthBounded_ {

template <typename Generator, typename ...Args>
struct SubtreeTask;

template <typename Generator, typename ...Args>
struct TestAct;

}

// This skeleton allows spawning all tasks into a workqueue based policy based on some depth limit
template <typename Generator, typename ...Args>
struct DepthBounded {
  typedef typename Generator::Nodetype Node;
  typedef typename Generator::Spacetype Space;

  typedef typename API::skeleton_signature::bind<Args...>::type args;

  static constexpr bool isCountNodes = parameter::value_type<args, API::tag::CountNodes_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isOptimisation = parameter::value_type<args, API::tag::Optimisation_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isDecision = parameter::value_type<args, API::tag::Decision_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isDepthLimited = parameter::value_type<args, API::tag::DepthLimited_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool pruneLevel = parameter::value_type<args, API::tag::PruneLevel_, std::integral_constant<bool, false> >::type::value;

  typedef typename parameter::value_type<args, API::tag::Verbose_, std::integral_constant<unsigned, 0> >::type Verbose;
  static constexpr unsigned verbose = Verbose::value;

  typedef typename parameter::value_type<args, API::tag::Metrics_, std::integral_constant<unsigned, 0> >::type Metrics;
  static constexpr unsigned metrics = Metrics::value;

  typedef typename parameter::value_type<args, API::tag::BoundFunction, nullFn__>::type boundFn;
  typedef typename boundFn::return_type Bound;
  typedef typename parameter::value_type<args, API::tag::ObjectiveComparison, std::greater<Bound> >::type Objcmp;

  typedef typename parameter::value_type<args, API::tag::DepthBoundedPoolPolicy, Workstealing::Policies::DepthPoolPolicy>::type Policy;

  static void printSkeletonDetails(const API::Params<Bound> & params) {
    hpx::cout << "Skeleton Type: DepthBounded\n";
    hpx::cout << "d_cutoff: " << params.spawnDepth << "\n";
    hpx::cout << "CountNodes : " << std::boolalpha << isCountNodes << "\n";
    hpx::cout << "Optimisation: " << std::boolalpha << isOptimisation << "\n";
    hpx::cout << "Decision: " << std::boolalpha << isDecision << "\n";
    hpx::cout << "DepthLimited: " << std::boolalpha << isDepthLimited << "\n";
    if constexpr(!std::is_same<boundFn, nullFn__>::value) {
        hpx::cout << "Using Bounding: true\n";
        hpx::cout << "PruneLevel Optimisation: " << std::boolalpha << pruneLevel << "\n";
      } else {
      hpx::cout << "Using Bounding: false\n";
    }
    if constexpr (std::is_same<Policy, Workstealing::Policies::Workpool>::value) {
      hpx::cout << "Workpool: Deque\n";
    } else {
      hpx::cout << "Workpool: DepthPool\n";
    }
    hpx::cout << hpx::flush;
  }

  static void expandWithSpawns(const Space & space,
                               const Node & n,
                               const API::Params<Bound> & params,
                               std::vector<uint64_t> & counts,
                               std::vector<hpx::future<void> > & childFutures,
                               std::uint64_t & nodeCount,
                               std::uint64_t & backtracks,
                               const unsigned childDepth) {
    Generator newCands = Generator(space, n);
    auto reg = Registry<Space, Node, Bound>::gReg;

    if constexpr(isCountNodes) {
        counts[childDepth] += newCands.numChildren;
    }

    if constexpr(isDepthLimited) {
        if (childDepth == params.maxDepth) {
          return;
        }
    }

    for (auto i = 0; i < newCands.numChildren; ++i) {
      auto c = newCands.next();

      if constexpr(metrics) {
        ++nodeCount;
      }
      auto pn = ProcessNode<Space, Node, Args...>::processNode(params, space, c);
      if (pn == ProcessNodeRet::Exit) { return; }
      else if (pn == ProcessNodeRet::Break) { 
        if constexpr(metrics) {
          ++backtracks;
        }
        break; 
      }
      //default continue

      // Spawn new tasks for all children (that are still alive after pruning)
      childFutures.push_back(createTask(childDepth + 1, c));
    }
  }

  static void expandNoSpawns(const Space & space,
                             const Node & n,
                             const API::Params<Bound> & params,
                             std::vector<uint64_t> & counts,
                             std::uint64_t & nodeCount,
                             std::uint64_t & prunes,
                             std::uint64_t & backtracks,
                             const unsigned childDepth) {
    auto reg = Registry<Space, Node, Bound>::gReg;
    Generator newCands = Generator(space, n);
    if constexpr(isDecision) {
        if (reg->stopSearch) {
          return;
        }
      }

    if constexpr(isCountNodes) {
        counts[childDepth] += newCands.numChildren;
    }

    if constexpr(isDepthLimited) {
        if (childDepth == params.maxDepth) {
          return;
        }
    }

    for (auto i = 0; i < newCands.numChildren; ++i) {
      auto c = newCands.next();

      if constexpr(metrics) {
        ++nodeCount;
      }
      auto pn = ProcessNode<Space, Node, Args...>::processNode(params, space, c);
      if (pn == ProcessNodeRet::Exit) { return; }
      else if (pn == ProcessNodeRet::Prune) {
        if constexpr(metrics) {
          ++prunes;
        }
        continue;
      }
      else if (pn == ProcessNodeRet::Break) {
        if constexpr(metrics) {
          ++backtracks; 
        }
        break;
      }

      expandNoSpawns(space, c, params, counts, nodeCount, prunes, backtracks, childDepth + 1);
    }
  }
  
  static void subtreeTask(const Node taskRoot,
                          const unsigned childDepth,
                          const hpx::naming::id_type donePromiseId) {
    auto reg = Registry<Space, Node, Bound>::gReg;

    auto store = MetricStore::store;

    std::vector<std::uint64_t> countMap;
    if constexpr(isCountNodes) {
        countMap.resize(reg->params.maxDepth + 1);
    }

    std::vector<hpx::future<void> > childFutures;
    std::uint64_t nodeCount = 0, prunes = 0, backtracks = 0;

    std::chrono::time_point<std::chrono::steady_clock> t1;
    if constexpr(metrics) {
      t1 = std::chrono::steady_clock::now();
    }

    if (childDepth <= reg->params.spawnDepth) {
      expandWithSpawns(reg->space, taskRoot, reg->params, countMap, childFutures, nodeCount, backtracks, childDepth);
    } else {
      expandNoSpawns(reg->space, taskRoot, reg->params, countMap, nodeCount, prunes, backtracks, childDepth);
    }

    if constexpr(metrics) {
      auto t2 = std::chrono::steady_clock::now();
      auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
     	const std::uint64_t time = (std::uint64_t) diff.count();
      store->updateNodesVisited(childDepth, nodeCount);
      store->updateTimes(childDepth, time);
      store->updatePrunes(childDepth, prunes);
      store->updateBacktracks(childDepth, backtracks);
    }

    // Atomically updates the (process) local counter
    if constexpr (isCountNodes) {
      reg->updateCounts(countMap);
    }
    hpx::apply(hpx::util::bind([=](std::vector<hpx::future<void> > & futs) {
      hpx::wait_all(futs);
      hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(donePromiseId, true);
    },
    std::move(childFutures)));
  }

  static hpx::future<void> createTask(const unsigned childDepth,
                                      const Node & taskRoot) {
    hpx::lcos::promise<void> prom;
    auto pfut = prom.get_future();
    auto pid  = prom.get_id();

    DepthBounded_::SubtreeTask<Generator, Args...> t;
    hpx::util::function<void(hpx::naming::id_type)> task;
    task = hpx::util::bind(t, hpx::util::placeholders::_1, taskRoot, childDepth, pid);

    auto workPool = std::static_pointer_cast<Policy>(Workstealing::Scheduler::local_policy);
    if constexpr (std::is_same<Policy, Workstealing::Policies::Workpool>::value) {
      workPool->addwork(task);
    } else {
      workPool->addwork(task, childDepth - 1);
    }

    return pfut;
  }

  static auto search (const Space & space,
                      const Node & root,
                      const API::Params<Bound> params = API::Params<Bound>()) {

    std::chrono::time_point<std::chrono::steady_clock> t1;
    if constexpr(metrics) {
        t1 = std::chrono::steady_clock::now();
    }

    if constexpr(verbose) {
        printSkeletonDetails(params);
    }
    hpx::wait_all(hpx::lcos::broadcast<InitRegistryAct<Space, Node, Bound> >(
        hpx::find_all_localities(), space, root, params));

    if constexpr(metrics) {
      hpx::wait_all(hpx::lcos::broadcast<InitMetricStoreAct>(hpx::find_all_localities(), params.maxDepth));
    }

    Policy::initPolicy();

    auto threadCount = hpx::get_os_thread_count() == 1 ? 1 : hpx::get_os_thread_count() - 1;
    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::startSchedulers_act>(
        hpx::find_all_localities(), threadCount));

    if constexpr(isOptimisation || isDecision) {
      auto inc = hpx::new_<Incumbent>(hpx::find_here()).get();
      hpx::wait_all(hpx::lcos::broadcast<UpdateGlobalIncumbentAct<Space, Node, Bound> >(
          hpx::find_all_localities(), inc));
      initIncumbent<Space, Node, Bound, Objcmp, Verbose>(root, params.initialBound);
    }

    // Issue is updateCounts by the looks of things. Something probably isn't initialised correctly.
    createTask(1, root).get();

    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::stopSchedulers_act>(
        hpx::find_all_localities()));

    if constexpr(metrics) {
      auto t2 = std::chrono::steady_clock::now();
      auto diff = std::chrono::duration_cast<std::chrono::seconds>(t2 - t1);
      const std::uint64_t time = diff.count();
      hpx::cout << "CPU Time (Before collecting metrics) " << time << hpx::endl;
      printTimes(params.maxDepth);
      printPrunes(params.maxDepth);
      printNodeCounts(params.maxDepth);
      printBacktracks(params.maxDepth);
    }
    
    // Return the right thing
    if constexpr(isCountNodes) {
      auto vec = totalNodeCounts<Space, Node, Bound>(params.maxDepth);
      auto sum = std::accumulate(vec.begin(), vec.end(), 0);
      return totalNodeCounts<Space, Node, Bound>(params.maxDepth);
    } else if constexpr(isOptimisation || isDecision) {
      auto reg = Registry<Space, Node, Bound>::gReg;

      typedef typename Incumbent::GetIncumbentAct<Node, Bound, Objcmp, Verbose> getInc;
      return hpx::async<getInc>(reg->globalIncumbent).get();
    } else {
      static_assert(isCountNodes || isOptimisation || isDecision, "Please provide a supported search type: CountNodes, Optimisation, Decision");
    }
  }
};

namespace DepthBounded_{

template <typename Generator, typename ...Args>
struct SubtreeTask : hpx::actions::make_action<
  decltype(&DepthBounded<Generator, Args...>::subtreeTask),
  &DepthBounded<Generator, Args...>::subtreeTask,
  SubtreeTask<Generator, Args...>>::type {};

}}}

namespace hpx { namespace traits {

template <typename Generator, typename ...Args>
struct action_stacksize<YewPar::Skeletons::DepthBounded_::SubtreeTask<Generator, Args...> > {
  enum { value = threads::thread_stacksize_huge };
};

}}

#endif
