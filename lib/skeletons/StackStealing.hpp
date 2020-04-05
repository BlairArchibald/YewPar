#ifndef SKELETONS_STACKSTEAL_HPP
#define SKELETONS_STACKSTEAL_HPP

#include <iostream>
#include <vector>
#include <cstdint>

#include "API.hpp"

#include <hpx/lcos/broadcast.hpp>
#include <hpx/include/iostreams.hpp>

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

  static constexpr bool isCountNodes = parameter::value_type<args, API::tag::CountNodes_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isOptimisation = parameter::value_type<args, API::tag::Optimisation_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isDecision = parameter::value_type<args, API::tag::Decision_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isDepthBounded = parameter::value_type<args, API::tag::DepthLimited_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool pruneLevel = parameter::value_type<args, API::tag::PruneLevel_, std::integral_constant<bool, true> >::type::value;
  static constexpr unsigned maxStackDepth = parameter::value_type<args, API::tag::MaxStackDepth, std::integral_constant<unsigned, 5000> >::type::value;

  typedef typename parameter::value_type<args, API::tag::Verbose_, std::integral_constant<unsigned, 0> >::type Verbose;
  static constexpr unsigned verbose = Verbose::value;
 
  // EXTENSION
  typedef typename parameter::value_type<args, API::tag::NodeCounts_, std::integral_constant<unsigned, 0> >::type NodeCounts;
  static constexpr unsigned nodeCounts = NodeCounts::value;

  // EXTENSION
  typedef typename parameter::value_type<args, API::tag::Regularity_, std::integral_constant<unsigned, 0> >::type Regularity;
  static constexpr unsigned regularity = Regularity::value;

  // EXTENSION
  typedef typename parameter::value_type<args, API::tag::Backtracks_, std::integral_constant<unsigned, 0> >::type Backtracks;
  static constexpr unsigned countBacktracks = Backtracks::value;

  typedef typename parameter::value_type<args, API::tag::BoundFunction, nullFn__>::type boundFn;
  typedef typename boundFn::return_type Bound;
  typedef typename parameter::value_type<args, API::tag::ObjectiveComparison, std::greater<Bound> >::type Objcmp;

  static void printSkeletonDetails(const API::Params<Bound> & params) {
    hpx::cout << "Skeleton Type: StackStealing\n";
    hpx::cout << "CountNodes : " << std::boolalpha << isCountNodes << "\n";
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
    hpx::cout << hpx::flush;
  }

  static void subTreeTask(const Node initNode,
                          const unsigned depth,
                          const hpx::naming::id_type donePromise) {
    auto reg = Registry<Space, Node, Bound>::gReg;
    std::vector<std::uint64_t> cntMap;

    // Setup the stack with root node
    StackElem<Generator> rootElem(reg->space, initNode);

    GeneratorStack<Generator> generatorStack(maxStackDepth, rootElem);
    if constexpr (isCountNodes) {
        cntMap.resize(reg->params.maxDepth + 1);
    }

    if constexpr (isCountNodes) {
      cntMap[depth] += rootElem.gen.numChildren;
    }

    // Register with the Policy to allow stealing from this stack
    std::shared_ptr<SharedState> stealReq;
    unsigned threadId;
    std::tie(stealReq, threadId) = std::static_pointer_cast<Policy>(Workstealing::Scheduler::local_policy)->registerThread();

    runTaskFromStack(depth, reg->space, generatorStack, stealReq, cntMap, donePromise, threadId);

  }

  using SubTreeTask = func<
    decltype(&StackStealing<Generator, Args...>::subTreeTask),
    &StackStealing<Generator, Args...>::subTreeTask>;

  using Policy      = Workstealing::Policies::SearchManager::SearchManagerComp<Node, SubTreeTask, Args...>;
  using Response    = typename Policy::Response_t;
  using SharedState = typename Policy::SharedState_t;

  // Find the required depth<Generator> to create "totalThreads" tasks
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
                                             YewPar::Skeletons::API::ObjectiveComparison<Objcmp>,
                                             YewPar::Skeletons::API::DepthLimited>
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
                           GeneratorStack<Generator> & generatorStack,
                           std::shared_ptr<SharedState> stealRequest,
                           std::vector<std::uint64_t> & cntMap,
                           std::vector<hpx::future<void> > & futures,
                           std::uint64_t & nodeCount,
                           std::uint64_t & prunes,
                           std::uint64_t & backtracks,
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
            if (reg->params.stealAll) {
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
              // Steal the first task only
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
        generatorStack[stackDepth + 1].node = generatorStack[stackDepth].gen.next();
        auto & child = generatorStack[stackDepth + 1].node;

        generatorStack[stackDepth].seen++;

        auto pn = ProcessNode<Space, Node, Args...>::processNode(reg->params, space, child);
        
        // EXTENSION
        if constexpr(nodeCounts) {
					nodeCount++;
				}
        // END EXTENSION

        if (pn == ProcessNodeRet::Exit) { return; }
        else if (pn == ProcessNodeRet::Prune) { continue; }
        else if (pn == ProcessNodeRet::Break) {
          stackDepth--;
          // EXTENSION
          if constexpr(countBacktracks) {
				  	backtracks++;
					}
          // END EXTENSION
          depth--;
          continue;
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
              // EXTENSION
              if constexpr(countBacktracks) {
								backtracks++;
							}
              // END EXTENSION
              depth--;
              continue;
          }
        }

        generatorStack[stackDepth].seen = 0;
        generatorStack[stackDepth].gen = childGen;
      } else {
        stackDepth--;
        depth--;
        // EXTENSION
        if constexpr(countBacktracks) {
					backtracks++;
				}
        // END EXTENSION
      }
    }
  }


  static void runTaskFromStack (const unsigned startingDepth,
                                const Space & space,
                                GeneratorStack<Generator> & generatorStack,
                                const std::shared_ptr<SharedState> stealRequest,
                                std::vector<std::uint64_t> & cntMap,
                                const hpx::naming::id_type donePromise,
                                const unsigned searchManagerId,
                                const int stackDepth = 0,
                                const int depth = -1) {
    auto reg = Registry<Space, Node, Bound>::gReg;

    auto store = MetricStore::store;

    std::vector<hpx::future<void> > futures;

    // EXTENSION
    std::uint64_t nodeCount = 0, prunes = 0, backtracks = 0;
    std::chrono::time_point<std::chrono::steady_clock> t1;

		if constexpr(regularity) {
    	t1 = std::chrono::steady_clock::now();
    }
    // END EXTENSION
		runWithStack(startingDepth, space, generatorStack, stealRequest, cntMap, futures, nodeCount, prunes, backtracks, stackDepth, depth);

    // EXTENSION
    if constexpr(nodeCounts) {
      store->updateNodesVisited(depth >= 0 ? depth : 0, nodeCount);
    }

    if constexpr(countBacktracks) {
      store->updateBacktracks(depth >= 0 ? depth : 0, backtracks);
    }

    if constexpr(regularity) {
      auto t2 = std::chrono::steady_clock::now();
      auto diff = t2-t1;
      const auto time = (const std::uint64_t) diff.count();
      hpx::apply(hpx::util::bind([=]() {
        store->updateTimes(depth >= 0 ? depth : 0, time);
      }));
    }
    // END EXTENSION
   
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
    hpx::util::function<void(),false> fn = hpx::util::bind(SubTreeTask::fn_ptr(), initNode, depth, donePromise);
    auto f = hpx::util::bind(&Workstealing::Scheduler::scheduler, fn);
    exe.add(f);
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
                               GeneratorStack<Generator> & generatorStack,
                               std::vector<std::uint64_t> & countMap,
                               std::vector<hpx::future<void> > & futures){
    auto localities = util::findOtherLocalities();
    localities.push_back(hpx::find_here());

    auto tasksSpawned = 0;
    while (stackDepth >= 0) {
      // If there's still children at this stackDepth we move into them
      if (generatorStack[stackDepth].seen < generatorStack[stackDepth].gen.numChildren) {

        // Get the next child at this stackDepth
        generatorStack[stackDepth + 1].node = generatorStack[stackDepth].gen.next();
        auto & child = generatorStack[stackDepth + 1].node;

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

          generatorStack[stackDepth].seen = 0;
          generatorStack[stackDepth].gen = childGen;
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
    StackElem<Generator> rootElem(space, root);
    // rootElem.seen = 0;
    // rootElem.node = root;
    // rootElem.gen = Generator(space, rootElem.node);

    GeneratorStack<Generator> genStack(maxStackDepth, rootElem);

    std::vector<std::uint64_t> countMap;
    if constexpr (isCountNodes) {
        countMap.resize(params.maxDepth + 1);
        countMap[1] += rootElem.gen.numChildren;
    }

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
      hpx::util::function<void(), false> fn = hpx::util::bind(&runTaskFromStack, 1, space, genStack, stealRequest, countMap, pid, std::get<1>(searchMgrInfo), stackDepth, depth);
      auto f = hpx::util::bind(&Workstealing::Scheduler::scheduler, fn);
      exe.add(f);
    }

    futures.push_back(std::move(f));
    hpx::wait_all(futures);
  }

  static auto search (const Space & space,
                      const Node & root,
                      const API::Params<Bound> params = API::Params<Bound>()) {

    if constexpr(verbose) {
      printSkeletonDetails(params);
    }

    hpx::wait_all(hpx::lcos::broadcast<InitRegistryAct<Space, Node, Bound> >(
        hpx::find_all_localities(), space, root, params));

    // EXTENSION
    if constexpr(nodeCounts || countBacktracks || regularity) {
      hpx::wait_all(hpx::lcos::broadcast<InitMetricStoreAct>(hpx::find_all_localities()));
    }
    // END EXTENSION

    Policy::initPolicy();

    if constexpr(isOptimisation || isDecision) {
      auto inc = hpx::new_<Incumbent>(hpx::find_here()).get();
      hpx::wait_all(hpx::lcos::broadcast<UpdateGlobalIncumbentAct<Space, Node, Bound> >(
          hpx::find_all_localities(), inc));
      initIncumbent<Space, Node, Bound, Objcmp, Verbose>(root, params.initialBound);
    }

    // EXTENSION
    std::chrono::time_point<std::chrono::steady_clock> t1;
    if constexpr(nodeCounts || countBacktracks) {
      t1 = std::chrono::steady_clock::now();
    }
    // END EXTENSION

    doSearch(space, root, params);

    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::stopSchedulers_act>(
        hpx::find_all_localities()));

    hpx::cout << hpx::flush;

    if (verbose >= 3) {
      for (const auto &l : hpx::find_all_localities()) {
        // We don't broadcast here to avoid racy output.
        hpx::async<Workstealing::Policies::SearchManagerPerf::printChunkSizeList_act>(l).get();
      }
    }

    // EXTENSION
		if constexpr(regularity) {
			for (const auto &l : hpx::find_all_localities()) {
				hpx::async<PrintTimesAct>(l).get();
			}
		}

    if constexpr(nodeCounts || countBacktracks) {
      auto t2 = std::chrono::steady_clock::now();
      auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
      const std::uint64_t time = diff.count();
      hpx::cout << "CPU Time (Before collecting metrics) " << time << hpx::endl;
      if constexpr(nodeCounts) {
        printNodeCounts();
      }
      if constexpr(countBacktracks) {
        printBacktracks();
      }
    }
    // END EXTENSION

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
};

}}


#endif
