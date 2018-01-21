#ifndef SKELETONS_STACKSTEAL_HPP
#define SKELETONS_STACKSTEAL_HPP

#include <vector>
#include <cstdint>

#include "API.hpp"

#include <hpx/lcos/broadcast.hpp>

#include "util/NodeGenerator.hpp"
#include "util/Registry.hpp"
#include "util/Incumbent.hpp"
#include "util/func.hpp"
#include "util/doubleWritePromise.hpp"
#include "util/util.hpp"

#include "workstealing/Scheduler.hpp"
#include "workstealing/policies/SearchManager.hpp"

#include "skeletons/Seq.hpp"

namespace YewPar { namespace Skeletons {

template <typename Generator, typename ...Args>
struct StackStealing {
  typedef typename Generator::Nodetype Node;
  typedef typename Generator::Spacetype Space;

  typedef typename API::skeleton_signature::bind<Args...>::type args;

  static constexpr bool isCountNodes = parameter::value_type<args, API::tag::CountNodes_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isBnB = parameter::value_type<args, API::tag::BnB_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isDecision = parameter::value_type<args, API::tag::Decision_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isDepthBounded = parameter::value_type<args, API::tag::DepthBounded_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool pruneLevel = parameter::value_type<args, API::tag::PruneLevel_, std::integral_constant<bool, false> >::type::value;
  static constexpr unsigned maxStackDepth = parameter::value_type<args, API::tag::MaxStackDepth, std::integral_constant<unsigned, 5000> >::type::value;
  typedef typename parameter::value_type<args, API::tag::BoundFunction, nullFn__>::type boundFn;
  typedef typename boundFn::return_type Bound;

  static void printSkeletonDetails() {
    std::cout << "Skeleton Type: StackStealing\n";
    std::cout << "CountNodes : " << std::boolalpha << isCountNodes << "\n";
    std::cout << "BNB: " << std::boolalpha << isBnB << "\n";
    std::cout << "Decision: " << std::boolalpha << isDecision << "\n";
    std::cout << "DepthBounded: " << std::boolalpha << isDepthBounded << "\n";
    std::cout << "MaxStackDepth: " << maxStackDepth << "\n";
    if constexpr(!std::is_same<boundFn, nullFn__>::value) {
        std::cout << "Using Bounding: true\n";
        std::cout << "PruneLevel Optimisation: " << std::boolalpha << pruneLevel << "\n";
      } else {
      std::cout << "Using Bounding: false\n";
    }
  }

  struct StackElem {
    unsigned seen;
    Generator gen;

    StackElem(Generator gen) : seen(0), gen(gen) {};
  };

  using GeneratorStack = std::vector<StackElem>;

  static void subTreeTask(const Node initNode,
                          const unsigned depth,
                          const hpx::naming::id_type donePromise) {
    auto reg = Registry<Space, Node, Bound>::gReg;
    std::vector<std::uint64_t> cntMap;

    // Setup the stack with root node
    auto rootGen = Generator(reg->space, initNode);

    GeneratorStack generatorStack(maxStackDepth, StackElem(rootGen));
    if constexpr (isCountNodes) {
        cntMap.resize(reg->params.maxDepth + 1);
      }

    if constexpr (isCountNodes) {
      cntMap[depth] += rootGen.numChildren;
    }

    generatorStack[0] = StackElem(rootGen);

    // Register with the Policy to allow stealing from this stack
    std::shared_ptr<SharedState> stealReq;
    unsigned threadId;
    std::tie(stealReq, threadId) = std::static_pointer_cast<Policy>(Workstealing::Scheduler::local_policy)->registerThread();

    runTaskFromStack(depth, reg->space, generatorStack, stealReq, cntMap, donePromise, threadId);
  }

  using SubTreeTask = func<
    decltype(&StackStealing<Generator, Args...>::subTreeTask),
    &StackStealing<Generator, Args...>::subTreeTask>;

  using Policy      = Workstealing::Policies::SearchManager<Node, SubTreeTask>;
  using Response    = typename Policy::Response_t;
  using SharedState = typename Policy::SharedState_t;

  // TODO: Duplicated in DepthSpawning
  static void updateIncumbent(const Node & node, const Bound & bnd) {
    auto reg = Registry<Space, Node, Bound>::gReg;
    // TODO: Should we force this local update for performance?
    //reg->updateRegistryBound(bnd)
    hpx::lcos::broadcast<UpdateRegistryBoundAct<Space, Node, Bound> >(
        hpx::find_all_localities(), bnd);

    typedef typename Incumbent<Node>::UpdateIncumbentAct act;
    hpx::async<act>(reg->globalIncumbent, node).get();
  }

  // Find the required depth to create "totalThreads" tasks
  static unsigned getRequiredSpawnDepth(const Space & space,
                                        const Node & root,
                                        const YewPar::Skeletons::API::Params<Bound> params,
                                        const unsigned totalThreads) {

    auto depthRequired = 1;
    while (depthRequired <= params.maxDepth) {
      auto localParams = params;
      localParams.maxDepth = depthRequired;
      auto numNodes = YewPar::Skeletons::Seq<Generator,
                                             YewPar::Skeletons::API::CountNodes,
                                             YewPar::Skeletons::API::BoundFunction<boundFn>,
                                             YewPar::Skeletons::API::DepthBounded>
                      ::search(space, root, localParams);
      if (numNodes[depthRequired] >= totalThreads) {
        break;
      }
      ++depthRequired;
    }
    return depthRequired;
  }

  // TODO: We only need the depth for counting so need to constexpr more
  static void runWithStack(const int startingDepth,
                           const Space & space,
                           GeneratorStack & generatorStack,
                           std::shared_ptr<SharedState> stealRequest,
                           std::vector<std::uint64_t> & cntMap,
                           std::vector<hpx::future<void> > & futures,
                           int stackDepth = 0,
                           int depth = -1) {
    auto reg = Registry<Space, Node, Bound>::gReg;
    std::vector<hpx::promise<void> > promises;

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
            // Steal it all
            if (std::get<2>(*stealRequest)) {
              Response res;
              while (generatorStack[i].seen < generatorStack[i].gen.numChildren) {
                generatorStack[i].seen++;

                promises.emplace_back();
                auto & prom = promises.back();

                futures.push_back(prom.get_future());

                const auto stolenSol = generatorStack[i].gen.next();
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

              const auto stolenSol = generatorStack[i].gen.next();
              Response res {hpx::util::make_tuple(stolenSol, startingDepth + i + 1, prom.get_id())};
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

        if constexpr(isDecision) {
          if (child.getObj() == reg->params.expectedObjective) {
            updateIncumbent(child, child.getObj());
            hpx::lcos::broadcast<SetStopFlagAct<Space, Node, Bound> >(hpx::find_all_localities());
            return;
          }
        }

        // Do we support bounding?
        if constexpr(!std::is_same<boundFn, nullFn__>::value) {
            auto bnd  = boundFn::invoke(space, child);
            if constexpr(isDecision) {
                if (bnd < reg->params.expectedObjective) {
                  if constexpr(pruneLevel) {
                      stackDepth--;
                      depth--;
                      continue;
                    } else {
                    continue;
                  }
                }
              // B&B Case
              } else {
              auto best = reg->localBound.load();
              if (bnd <= best) {
                if constexpr(pruneLevel) {
                    stackDepth--;
                    depth--;
                    continue;
                  } else {
                  continue;
                }
              }
            }
          }

        if constexpr(isBnB) {
          // FIXME: unsure about loading this twice in terms of performance
          auto best = reg->localBound.load();
          if (child.getObj() > best) {
            updateIncumbent(child, child.getObj());
          }
        }

        // Get the child's generator
        const auto childGen = Generator(space, child);

        // Going down
        stackDepth++;
        depth++;

        if constexpr(isCountNodes) {
            cntMap[depth] += childGen.numChildren;
        }

        if constexpr(isDepthBounded) {
            // This doesn't look quite right to me, we want the next element at this level not the previous?
            if (depth == reg->params.maxDepth) {
              stackDepth--;
              depth--;
              continue;
          }
        }

        generatorStack[stackDepth] = StackElem(childGen);
      } else {
        stackDepth--;
        depth--;
      }
    }
  }


  static void runTaskFromStack (const unsigned startingDepth,
                                const Space & space,
                                GeneratorStack & generatorStack,
                                const std::shared_ptr<SharedState> stealRequest,
                                std::vector<std::uint64_t> & cntMap,
                                const hpx::naming::id_type donePromise,
                                const unsigned searchManagerId,
                                const int stackDepth = 0,
                                const int depth = -1) {
    auto reg = Registry<Space, Node, Bound>::gReg;
    std::vector<hpx::future<void> > futures;

    runWithStack(startingDepth, space, generatorStack, stealRequest, cntMap, futures, stackDepth, depth);

    // Atomically updates the (process) local counter
    if constexpr(isCountNodes) {
        reg->updateCounts(cntMap);
    }

    std::static_pointer_cast<Policy>(Workstealing::Scheduler::local_policy)->unregisterThread(searchManagerId);

    hpx::apply(hpx::util::bind([=](std::vector<hpx::future<void> > & futs) {
          hpx::wait_all(futs);
          hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(donePromise, true);
        }, std::move(futures)));
  }


  // Action to push a new scheduler running this skeleton to a distributed node
  // (for setting initial work distribution)
  static void addWork (const Node initNode,
                       const unsigned depth,
                       const hpx::naming::id_type donePromise) {
    hpx::threads::executors::default_executor exe(hpx::threads::thread_priority_critical,
                                                  hpx::threads::thread_stacksize_huge);
    hpx::apply(exe, &Workstealing::Scheduler::scheduler, hpx::util::bind(SubTreeTask::fn_ptr(), initNode, depth, donePromise));
  }
  struct addWorkAct : hpx::actions::make_action<
    decltype(&StackStealing<Generator, Args...>::addWork),
    &StackStealing<Generator, Args...>::addWork,
    addWorkAct>::type {};

  static void spawnInitialWork(const unsigned depthRequired,
                               const unsigned tasksRequired,
                               int & stackDepth,
                               int & depth,
                               const Space & space,
                               GeneratorStack & generatorStack,
                               std::vector<std::uint64_t> & countMap,
                               std::vector<hpx::future<void> > & futures){
    auto localities = util::findOtherLocalities();
    localities.push_back(hpx::find_here());

    auto tasksSpawned = 0;
    while (stackDepth >= 0) {
      // If there's still children at this stackDepth we move into them
      if (generatorStack[stackDepth].seen < generatorStack[stackDepth].gen.numChildren) {
        // Get the next child at this stackDepth
        const auto child = generatorStack[stackDepth].gen.next();
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
          tasksSpawned++;

          // We keep a spare thread for ourselves to execute as
          if (tasksSpawned == tasksRequired) {
            break;
          }
        } else {
          // Get the child's generator
          const auto childGen = Generator(space, child);
          if constexpr(isCountNodes) {
              countMap[depth] += childGen.numChildren;
          }

          generatorStack[stackDepth] = StackElem(childGen);
        }
      } else {
        stackDepth--;
        depth--;
      }
    }
  }

  static void doSearch(const Space & space,
                       const Node & root,
                       const API::Params<Bound> & params) {

    // FIXME: Assumes homogeneous machines
    unsigned totalThreads = 0;
    if (hpx::get_os_thread_count() == 1) {
      totalThreads = hpx::find_all_localities().size();
    } else {
      totalThreads = hpx::find_all_localities().size() * (hpx::get_os_thread_count() - 1);
    }

    // Master stack
    auto rootGen = Generator(space, root);

    GeneratorStack genStack(maxStackDepth, StackElem(rootGen));

    std::vector<std::uint64_t> countMap;
    if constexpr (isCountNodes) {
        countMap.resize(params.maxDepth + 1);
        countMap[1] += rootGen.numChildren;
    }

    genStack[0].seen = 0;
    genStack[0].gen  = rootGen;

    auto stackDepth = 0;
    auto depth = 1;

    std::vector<hpx::future<void> > futures;
    if (totalThreads > 1) {
      auto depthRequired = getRequiredSpawnDepth(space, root, params, totalThreads);
      spawnInitialWork(depthRequired, totalThreads - 1, stackDepth, depth, space, genStack, countMap, futures);
    }

    // Register the rest of the work from the main thread with the search manager
    auto searchMgrInfo = std::static_pointer_cast<Policy>(Workstealing::Scheduler::local_policy)->registerThread();
    auto stealRequest  = std::get<0>(searchMgrInfo);

    // Continue the actual work
    hpx::promise<void> prom;
    auto f = prom.get_future();
    auto pid = prom.get_id();

    // Launch initialising thread as a new Scheduler
    if (totalThreads == 1) {
      runTaskFromStack(1, space, genStack, stealRequest, countMap, pid, std::get<1>(searchMgrInfo), stackDepth, depth);
    } else {
      hpx::threads::executors::default_executor exe(hpx::threads::thread_priority_critical,
                                                    hpx::threads::thread_stacksize_huge);
      hpx::apply(exe, &Workstealing::Scheduler::scheduler,
                hpx::util::bind(&runTaskFromStack, 1, space, genStack, stealRequest, countMap, pid, std::get<1>(searchMgrInfo), stackDepth, depth));
    }

    futures.push_back(std::move(f));
    hpx::wait_all(futures);
  }

  // TODO: This is repeated elsewhere
  static std::vector<std::uint64_t> totalNodeCounts(const unsigned maxDepth) {
    auto cntList = hpx::lcos::broadcast<GetCountsAct<Space, Node, Bound> >(
        hpx::find_all_localities()).get();

    std::vector<std::uint64_t> res(maxDepth + 1);
    for (auto i = 0; i <= maxDepth; ++i) {
      for (const auto & cnt : cntList) {
        res[i] += cnt[i];
      }
    }
    res[0] = 1; //Account for root node
    return res;
  }

  static auto search (const Space & space,
                      const Node & root,
                      const API::Params<Bound> params = API::Params<Bound>()) {
    hpx::wait_all(hpx::lcos::broadcast<InitRegistryAct<Space, Node, Bound> >(
        hpx::find_all_localities(), space, root, params));

    Policy::initPolicy(params.stealAll);

    if constexpr(isBnB || isDecision) {
      auto inc = hpx::new_<Incumbent<Node> >(hpx::find_here()).get();
      hpx::wait_all(hpx::lcos::broadcast<UpdateGlobalIncumbentAct<Space, Node, Bound> >(
          hpx::find_all_localities(), inc));
      updateIncumbent(root, root.getObj());
    }

    doSearch(space, root, params);

    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::stopSchedulers_act>(
        hpx::find_all_localities()));

    // Return the right thing
    if constexpr(isCountNodes) {
      return totalNodeCounts(params.maxDepth);
    } else if constexpr(isBnB || isDecision) {
      auto reg = Registry<Space, Node, Bound>::gReg;

      typedef typename Incumbent<Node>::GetIncumbentAct getInc;
      return hpx::async<getInc>(reg->globalIncumbent).get();
    } else {
      static_assert(isCountNodes || isBnB || isDecision, "Please provide a supported search type: CountNodes, BnB, Decision");
    }
  }
};

}}


#endif
