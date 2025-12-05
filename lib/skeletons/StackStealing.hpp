#ifndef SKELETONS_STACKSTEAL_HPP
#define SKELETONS_STACKSTEAL_HPP

#include <iostream>
#include <vector>
#include <cstdint>

#include "API.hpp"

#include <hpx/collectives/broadcast.hpp>
#include <hpx/iostream.hpp>

#include <boost/format.hpp>

#include "util/NodeGenerator.hpp"
#include "util/Registry.hpp"
#include "util/Incumbent.hpp"
#include "util/func.hpp"
#include "util/util.hpp"

#include "Common.hpp"

#include "workstealing/Scheduler.hpp"
#include "workstealing/policies/SearchManager.hpp"

#include "skeletons/Seq.hpp"

namespace YewPar { namespace Skeletons {

template <typename Generator, typename ...Args>
struct StackStealing {
  typedef typename Generator::Nodetype Node;
  typedef typename Generator::Spacetype Space;

  typedef typename API::skeleton_signature::bind<Args...>::type args;

  static constexpr bool isEnumeration = parameter::value_type<args, API::tag::Enumeration_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isOptimisation = parameter::value_type<args, API::tag::Optimisation_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isDecision = parameter::value_type<args, API::tag::Decision_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isDepthBounded = parameter::value_type<args, API::tag::DepthLimited_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool pruneLevel = parameter::value_type<args, API::tag::PruneLevel_, std::integral_constant<bool, false> >::type::value;
  static constexpr unsigned maxStackDepth = parameter::value_type<args, API::tag::MaxStackDepth, std::integral_constant<unsigned, 5000> >::type::value;

  typedef typename parameter::value_type<args, API::tag::Verbose_, std::integral_constant<unsigned, 0> >::type Verbose;
  static constexpr unsigned verbose = Verbose::value;

  typedef typename parameter::value_type<args, API::tag::BoundFunction, nullFn__>::type boundFn;
  typedef typename boundFn::return_type Bound;
  typedef typename parameter::value_type<args, API::tag::ObjectiveComparison, std::greater<Bound> >::type Objcmp;
  typedef typename parameter::value_type<args, API::tag::Enumerator, IdentityEnumerator<Node>>::type Enum;

  static void printSkeletonDetails(const API::Params<Bound> & params) {
    hpx::cout << "Skeleton Type: StackStealing\n";
    hpx::cout << "Enumeration : " << std::boolalpha << isEnumeration << "\n";
    hpx::cout << "Optimisation: " << std::boolalpha << isOptimisation << "\n";
    hpx::cout << "Decision: " << std::boolalpha << isDecision << "\n";
    hpx::cout << "DepthBounded: " << std::boolalpha << isDepthBounded << "\n";
    hpx::cout << "MaxStackDepth: " << maxStackDepth << "\n";
    if constexpr(!std::is_same<boundFn, nullFn__>::value) {
        hpx::cout << "Using Bounding: true\n";
        hpx::cout << "PruneLevel Optimisation: " << std::boolalpha << pruneLevel << "\n";
    } else {
      hpx::cout << "Using Bounding: false\n";
    }
    hpx::cout << "Chunking Enabled: " << std::boolalpha << params.stealAll << "\n";
    hpx::cout << std::flush;
  }

  static void subTreeTask(const Node initNode,
                          const unsigned depth,
                          const hpx::id_type donePromise) {
    auto reg = Registry<Space, Node, Bound, Enum>::gReg;

    Enum acc;

    if constexpr(isEnumeration) {
        acc.accumulate(initNode);
    }

    // Setup the stack with root node
    GeneratorStack<Generator> genStack;
    genStack.reserve(maxStackDepth);
    genStack.emplace_back(reg->space, initNode);

    // Register with the Policy to allow stealing from this stack
    std::shared_ptr<SharedState> stealReq;
    unsigned threadId;
    auto searchMgr = std::static_pointer_cast<Policy>(Workstealing::Scheduler::local_policy);
    std::tie(stealReq, threadId) = searchMgr->registerThread();

    std::vector<hpx::future<void> > futures;
    runWithStack(depth, genStack, stealReq, acc, futures);

    searchMgr->unregisterThread(threadId);

    // Atomically updates the (process) local counter
    if constexpr(isEnumeration) {
        reg->updateEnumerator(acc);
    }

    termination_wait_act act;
    hpx::post(act, hpx::find_here(), std::move(futures), donePromise);
  }

  using SubTreeTask = func<
    decltype(&StackStealing<Generator, Args...>::subTreeTask),
    &StackStealing<Generator, Args...>::subTreeTask>;

  using Policy      = Workstealing::Policies::SearchManager::SearchManagerComp<Node, SubTreeTask, Args...>;
  using Response    = typename Policy::Response_t;
  using SharedState = typename Policy::SharedState_t;

  // TODO: We only need the depth for counting so need to constexpr more
  static void runWithStack(const int startingDepth,
                           GeneratorStack<Generator> & generatorStack,
                           std::shared_ptr<SharedState> stealRequest,
                           Enum & acc,
                           std::vector<hpx::future<void> > & futures,
                           int stackDepth = 0,
                           int depth = -1) {
    auto reg = Registry<Space, Node, Bound, Enum>::gReg;
    std::vector<hpx::distributed::promise<void> > promises;

    // We do this because arguments can't default initialise to themselves
    if (depth == -1) {
      depth = startingDepth;
    }

    while (stackDepth >= 0) {

      if constexpr(isDecision) {
        if (reg->stopSearch) {
          return;
        }
      }

      // Handle steals first
      if (std::get<0>(*stealRequest)) {
        // We steal from the highest possible generator with work
        bool responded = false;
        for (auto i = 0; i < stackDepth; ++i) {
          // Work left at this level:
          if (generatorStack[i].seen < generatorStack[i].gen.numChildren) {
            if (reg->params.stealAll) {
              Response res;
              while (generatorStack[i].seen < generatorStack[i].gen.numChildren) {
                generatorStack[i].seen++;

                promises.emplace_back();
                auto & prom = promises.back();

                futures.push_back(prom.get_future());

                const auto stolenSol = generatorStack[i].gen.next();
                res.emplace_back(hpx::make_tuple(stolenSol, startingDepth + i + 1, prom.get_id()));
              }

              std::get<1>(*stealRequest).set(res);
              responded = true;
              break;
              // Steal the first task only
            } else {
              generatorStack[i].seen++;

              promises.emplace_back();
              auto & prom = promises.back();

              futures.push_back(prom.get_future());

              const auto stolenSol = generatorStack[i].gen.next();
              Response res {hpx::make_tuple(stolenSol, startingDepth + i + 1, prom.get_id())};
              std::get<1>(*stealRequest).set(res);

              responded = true;
              break;
            }
          }
        }
        if (!responded) {
          Response res;
          std::get<1>(*stealRequest).set(res);
        }
        std::get<0>(*stealRequest).store(false);
      }

      // If there's still children at this stackDepth we move into them
      if (generatorStack[stackDepth].seen < generatorStack[stackDepth].gen.numChildren) {
        // Get the next child at this stackDepth
        const auto child = generatorStack[stackDepth].gen.next();
        generatorStack[stackDepth].seen++;

        auto pn = ProcessNode<Space, Node, Args...>::processNode(reg->params, reg->space, child, acc);
        if (pn == ProcessNodeRet::Exit) { return; }
        else if (pn == ProcessNodeRet::Prune) { continue; }
        else if (pn == ProcessNodeRet::Break) {
          generatorStack.pop_back();
          stackDepth--;
          depth--;
          continue;
        }


        if constexpr(isDepthBounded) {
            if (depth == reg->params.maxDepth) {
              continue;
          }
        }

        // Going down
        stackDepth++;
        depth++;

        generatorStack.emplace_back(reg->space, child);
      } else {
        generatorStack.pop_back();
        stackDepth--;
        depth--;
      }
    }
  }


  // Action to push a new scheduler running this skeleton to a distributed node
  // (for setting initial work distribution)
  static hpx::future<void> addWork(const Node initNode,
                                   const unsigned depth) {
    hpx::distributed::promise<void> prom;
    auto pfut = prom.get_future();
    auto pid  = prom.get_id();

    auto localSearchMgr = std::static_pointer_cast<Policy>(Workstealing::Scheduler::local_policy);
    localSearchMgr->addWork(initNode, depth, pid);

    return pfut;
  }

  struct addWorkAct : hpx::actions::make_action<
    decltype(&StackStealing<Generator, Args...>::addWork),
    &StackStealing<Generator, Args...>::addWork,
    addWorkAct>::type {};

  static auto search (const Space & space,
                      const Node & root,
                      const API::Params<Bound> params = API::Params<Bound>()) {
    if constexpr(verbose) {
      printSkeletonDetails(params);
    }

    hpx::wait_all(hpx::lcos::broadcast<InitRegistryAct<Space, Node, Bound, Enum> >(
        hpx::find_all_localities(), space, root, params));

    Policy::initPolicy();

    if constexpr(isOptimisation || isDecision) {
      auto inc = hpx::new_<Incumbent>(hpx::find_here()).get();
      hpx::wait_all(hpx::lcos::broadcast<UpdateGlobalIncumbentAct<Space, Node, Bound, Enum> >(
          hpx::find_all_localities(), inc));
      initIncumbent<Space, Node, Bound, Enum, Objcmp, Verbose>(root, params.initialBound);
    }

    auto threadCount = hpx::get_os_thread_count() == 1 ? 1 : hpx::get_os_thread_count() - 1;
    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::startSchedulers_act>(
        hpx::find_all_localities(), threadCount));

    addWork(root, 0).get();

    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::stopSchedulers_act>(
        hpx::find_all_localities()));

    hpx::cout << std::flush;

    if (verbose >= 3) {
      for (const auto &l : hpx::find_all_localities()) {
        // We don't broadcast here to avoid racy output.
        hpx::async<Workstealing::Policies::SearchManagerPerf::printChunkSizeList_act>(l).get();
      }
    }

    // Return the right thing
    if constexpr(isEnumeration) {
      return combineEnumerators<Space, Node, Bound, Enum>();
    } else if constexpr(isOptimisation || isDecision) {
      auto reg = Registry<Space, Node, Bound, Enum>::gReg;

      typedef typename Incumbent::GetIncumbentAct<Node, Bound, Objcmp, Verbose> getInc;
      return hpx::async<getInc>(reg->globalIncumbent).get();
    } else {
      static_assert(isEnumeration || isOptimisation || isDecision, "Please provide a supported search type: Enumeration, Optimisation, Decision");
    }
  }
};

}}


#endif
