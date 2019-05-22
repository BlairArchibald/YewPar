#ifndef SKELETONS_ORDERED_HPP
#define SKELETONS_ORDERED_HPP

#include <iostream>
#include <vector>
#include <cstdint>

#include <hpx/lcos/broadcast.hpp>
#include <hpx/include/iostreams.hpp>

#include <boost/format.hpp>

#include "API.hpp"

#include "util/NodeGenerator.hpp"
#include "util/Registry.hpp"
#include "util/Incumbent.hpp"
#include "util/func.hpp"
#include "util/DistSetOnceFlag.hpp"

#include "Common.hpp"

#include "workstealing/policies/PriorityOrdered.hpp"

namespace YewPar { namespace Skeletons {

namespace Ordered_ {
template <typename Generator, typename ...Args>
struct SubtreeTask;
}

template <typename Generator, typename ...Args>
struct Ordered {
  typedef typename Generator::Nodetype Node;
  typedef typename Generator::Spacetype Space;

  typedef typename API::skeleton_signature::bind<Args...>::type args;

  static constexpr bool isCountNodes = parameter::value_type<args, API::tag::CountNodes_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isOptimisation = parameter::value_type<args, API::tag::Optimisation_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isDecision = parameter::value_type<args, API::tag::Decision_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isDepthBounded = parameter::value_type<args, API::tag::DepthLimited_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool pruneLevel = parameter::value_type<args, API::tag::PruneLevel_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool discrepancySearch = parameter::value_type<args, API::tag::DiscrepancySearch_, std::integral_constant<bool, false> >::type::value;

  typedef typename parameter::value_type<args, API::tag::Verbose_, std::integral_constant<unsigned, 0> >::type Verbose;
  static constexpr unsigned verbose = Verbose::value;

  typedef typename parameter::value_type<args, API::tag::BoundFunction, nullFn__>::type boundFn;
  typedef typename boundFn::return_type Bound;
  typedef typename parameter::value_type<args, API::tag::ObjectiveComparison, std::greater<Bound> >::type Objcmp;

  static void printSkeletonDetails() {
    hpx::cout << "Skeleton Type: Ordered\n";
    hpx::cout << "CountNodes : " << std::boolalpha << isCountNodes << "\n";
    hpx::cout << "Optimisation: " << std::boolalpha << isOptimisation << "\n";
    hpx::cout << "Decision: " << std::boolalpha << isDecision << "\n";
    hpx::cout << "DepthBounded: " << std::boolalpha << isDepthBounded << "\n";
    if constexpr(!std::is_same<boundFn, nullFn__>::value) {
        hpx::cout << "Using Bounding: true\n";
        hpx::cout << "PruneLevel Optimisation: " << std::boolalpha << pruneLevel << "\n";
      } else {
      hpx::cout << "Using Bounding: false\n";
    }
    hpx::cout << hpx::flush;
  }

  struct OrderedTask {
    OrderedTask(const Node n, unsigned priority) : node(n), priority(priority) {
      startedFlag = hpx::new_<YewPar::util::DistSetOnceFlag>(hpx::find_here()).get();
    };
    const Node node;
    unsigned priority;
    hpx::naming::id_type startedFlag;
  };

  // Spawn tasks in a discrepancy search fashion
  // Discrepancy search priority based on number of discrepancies taken
  // Invariant: spawnDepth > 0
  static std::vector<OrderedTask> prioritiseTasks(const Space & space,
                                                  unsigned spawnDepth,
                                                  const Node & root) {
    std::vector<OrderedTask> tasks;
    if constexpr (discrepancySearch) {
      std::function<void(unsigned, unsigned, const Node &)>
          fn = [&](unsigned depth, unsigned numDisc, const Node & n) {
        if (depth == 0) {
          tasks.emplace_back(OrderedTask(n, numDisc));
        } else {
          auto newCands = Generator(space, n);
          for (auto i = 0; i < newCands.numChildren; ++i) {
            auto node = newCands.next();
            fn(depth - 1, numDisc + i, node);
          }
        }
      };
      fn(spawnDepth, 0, root);
      // Linear task spawning
    } else {
      std::function<void(unsigned, const Node &)> fn =
          [&](unsigned depth, const Node & n) {
        if (depth == 0) {
          tasks.emplace_back(OrderedTask(n, 0));
        } else {
          auto newCands = Generator(space, n);
          for (auto i = 0; i < newCands.numChildren; ++i) {
            auto node = newCands.next();
            fn(depth - 1, node);
          }
        }
      };
      fn(spawnDepth, root);

      // Reassign priorities from 0 to get a fixed order
      for (auto i = 0; i < tasks.size(); ++i) {
        tasks[i].priority = i;
      }
    }

    if (verbose > 1) {
      hpx::cout <<
          (boost::format("Ordered Skeleton Spawned %1% Tasks\n") % tasks.size())
                << hpx::flush;
    }

    return tasks;
  }
  static void expandNoSpawns(const Space & space,
                             const Node & n,
                             const API::Params<Bound> & params,
                             std::vector<uint64_t> & counts,
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

    if constexpr(isDepthBounded) {
        if (childDepth == params.maxDepth) {
          return;
        }
    }

    for (auto i = 0; i < newCands.numChildren; ++i) {
      auto c = newCands.next();

      auto pn = ProcessNode<Space, Node, Args...>::processNode(params, space, c);
      if (pn == ProcessNodeRet::Exit) { return; }
      else if (pn == ProcessNodeRet::Break) { break; }

      expandNoSpawns(space, c, params, counts, childDepth + 1);
    }
  }

  static auto search (const Space & space,
                      const Node & root,
                      const API::Params<Bound> params = API::Params<Bound>()) {
    if constexpr(verbose) {
      printSkeletonDetails();
    }

    hpx::wait_all(hpx::lcos::broadcast<InitRegistryAct<Space, Node, Bound> >(
        hpx::find_all_localities(), space, root, params));

    if constexpr(isOptimisation || isDecision) {
      auto inc = hpx::new_<Incumbent>(hpx::find_here()).get();
      hpx::wait_all(hpx::lcos::broadcast<UpdateGlobalIncumbentAct<Space, Node, Bound> >(
          hpx::find_all_localities(), inc));
      initIncumbent<Space, Node, Bound, Objcmp, Verbose>(root, params.initialBound);
    }

    Workstealing::Policies::PriorityOrderedPolicy::initPolicy();

    auto spawn_start_time = std::chrono::steady_clock::now();
    // Spawn all tasks to some depth *ordered*
    auto tasks = prioritiseTasks(space, params.spawnDepth, root);
    for (auto const & t : tasks) {
      Ordered_::SubtreeTask<Generator, Args...> child;
      hpx::util::function<void(hpx::naming::id_type)> task;
      task = hpx::util::bind(child, hpx::util::placeholders::_1, t.node, t.startedFlag);
      std::static_pointer_cast<Workstealing::Policies::PriorityOrderedPolicy>
          (Workstealing::Scheduler::local_policy)->addwork(t.priority, std::move(task));
    }

    if (verbose > 1) {
      auto spawn_time = std::chrono::duration_cast<std::chrono::milliseconds>
          (std::chrono::steady_clock::now() - spawn_start_time);
      hpx::cout <<
          (boost::format("Ordered Skeleton, time to spawn tasks: %1% ms\n") % spawn_time.count())
                << hpx::flush;
    }

    // We need to start 1 less thread on the master locality than everywhere
    // else to handle the sequential order
    auto allLocs = hpx::find_all_localities();
    allLocs.erase(std::remove(allLocs.begin(), allLocs.end(), hpx::find_here()), allLocs.end());

    auto threadCount = hpx::get_os_thread_count() == 1 ? 1 : hpx::get_os_thread_count() - 1;
    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::startSchedulers_act>(allLocs, threadCount));

    auto threadCountLocal = hpx::get_os_thread_count() <= 2 ? 0 : hpx::get_os_thread_count() - 2;
    Workstealing::Scheduler::startSchedulers(threadCountLocal);

    // Make this thread the sequential thread of execution.
    auto reg = Registry<Space, Node, Bound>::gReg;
    for (auto & t : tasks) {
      // Allow early termination of sequential thread
      if constexpr(isDecision) {
        if (reg->stopSearch) {
          break;
        }
      }

      // Quick prune path to avoid writing global flags
      if constexpr(isOptimisation && !std::is_same<boundFn, nullFn__>::value) {
        Objcmp cmp;
        auto best = reg->localBound.load();
        auto bnd  = boundFn::invoke(space, t.node);
        if (!cmp(bnd,best)) {
          continue;
        }
      }

      auto weStarted = hpx::async<YewPar::util::DistSetOnceFlag::set_value_action>(t.startedFlag).get();
      if (weStarted) {
        std::vector<std::uint64_t> countMap;
        if constexpr(isCountNodes) {
          countMap.resize(reg->params.maxDepth + 1);
        }

        expandNoSpawns(space, t.node, params, countMap, params.spawnDepth);
      }
    }

    // We have either seen everything or terminated early to make sure everyone stops
    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::stopSchedulers_act>(
        hpx::find_all_localities()));

    // Return the right thing
    if constexpr(isCountNodes) {
      return totalNodeCounts<Space, Node, Bound>(params.maxDepth);
    } else if constexpr(isOptimisation || isDecision) {
      auto reg = Registry<Space, Node, Bound>::gReg;
      typedef typename Incumbent::GetIncumbentAct<Node, Bound, Objcmp, Verbose> getInc;
      return hpx::async<getInc>(reg->globalIncumbent).get();
    } else {
      static_assert(isCountNodes || isOptimisation || isDecision, "Please provide a supported search type: CountNodes, Optimisation, Decision");
    }
  }

  static void subtreeTask(const Node taskRoot,
                          const hpx::naming::id_type started) {
    // Don't bother checking if the sequential thread has done this task since we are stopping anyway
    auto reg = Registry<Space, Node, Bound>::gReg;
    if constexpr (isDecision) {
      if (reg->stopSearch) {
        return;
      }
    }

    // Quick prune path
    if constexpr(isOptimisation && !std::is_same<boundFn, nullFn__>::value) {
      Objcmp cmp;
      auto best = reg->localBound.load();
      auto bnd  = boundFn::invoke(reg->space, taskRoot);
      if (!cmp(bnd,best)) {
        return;
      }
    }

    auto weStarted = hpx::async<YewPar::util::DistSetOnceFlag::set_value_action>(started).get();
    // Sequential thread has beaten us to this task. Don't bother executing it again.
    if (weStarted) {
      std::vector<std::uint64_t> countMap;
      if constexpr(isCountNodes) {
        countMap.resize(reg->params.maxDepth + 1);
      }

      expandNoSpawns(reg->space, taskRoot, reg->params, countMap, reg->params.spawnDepth);
    }
  }
};

namespace Ordered_ {
template <typename Generator, typename ...Args>
struct SubtreeTask : hpx::actions::make_action<
  decltype(&Ordered<Generator, Args...>::subtreeTask),
  &Ordered<Generator, Args...>::subtreeTask,
  SubtreeTask<Generator, Args...>>::type {};
}

}}

namespace hpx { namespace traits {

template <typename Generator, typename ...Args>
struct action_stacksize<YewPar::Skeletons::Ordered_::SubtreeTask<Generator, Args...> > {
  enum { value = threads::thread_stacksize_huge };
};

}}

#endif
