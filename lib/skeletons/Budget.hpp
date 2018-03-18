#ifndef SKELETONS_BUDGET_HPP
#define SKELETONS_BUDGET_HPP

#include <hpx/include/iostreams.hpp>

namespace YewPar { namespace Skeletons {

namespace detail {
template <typename Generator, typename ...Args>
struct SubtreeTask;
}


template <typename Generator, typename ...Args>
struct Budget {
  typedef typename Generator::Nodetype Node;
  typedef typename Generator::Spacetype Space;

  typedef typename API::skeleton_signature::bind<Args...>::type args;

  static constexpr bool isCountNodes = parameter::value_type<args, API::tag::CountNodes_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isOptimisation = parameter::value_type<args, API::tag::Optimisation_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isDecision = parameter::value_type<args, API::tag::Decision_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isDepthBounded = parameter::value_type<args, API::tag::DepthBounded_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool pruneLevel = parameter::value_type<args, API::tag::PruneLevel_, std::integral_constant<bool, false> >::type::value;
  static constexpr unsigned maxStackDepth = parameter::value_type<args, API::tag::MaxStackDepth, std::integral_constant<unsigned, 5000> >::type::value;

  typedef typename parameter::value_type<args, API::tag::Verbose_, std::integral_constant<unsigned, 0> >::type Verbose;
  static constexpr unsigned verbose = Verbose::value;

  typedef typename parameter::value_type<args, API::tag::BoundFunction, nullFn__>::type boundFn;
  typedef typename boundFn::return_type Bound;
  typedef typename parameter::value_type<args, API::tag::ObjectiveComparison, std::greater<Bound> >::type Objcmp;

  typedef typename parameter::value_type<args, API::tag::DepthBoundedPoolPolicy, Workstealing::Policies::DepthPoolPolicy>::type Policy;

  static void printSkeletonDetails() {
    hpx::cout << "Skeleton Type: Budget\n";
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
    if constexpr (std::is_same<Policy, Workstealing::Policies::Workpool>::value) {
        hpx::cout << "Workpool: Deque\n";
      } else {
      hpx::cout << "Workpool: DepthPool\n";
    }
    hpx::cout << hpx::flush;
  }

  static void expand(const Space & space,
                     const Node & n,
                     const API::Params<Bound> & params,
                     std::vector<uint64_t> & counts,
                     std::vector<hpx::future<void> > & childFutures,
                     const unsigned childDepth) {
    auto reg = Registry<Space, Node, Bound>::gReg;

    auto depth = childDepth;
    auto backtracks = 0;

    // Init the stack
    StackElem<Generator> initElem(space, n);
    GeneratorStack<Generator> genStack(maxStackDepth, initElem);


    auto stackDepth = 0;
    while (stackDepth >= 0) {

      if constexpr(isDecision) {
        if (reg->stopSearch) {
          return;
        }
      }

      // We spawn when we have exhausted our backtrack budget
      if (backtracks == params.backtrackBudget) {
        // Spawn everything at the highest possible depth
        for (auto i = 0; i < stackDepth; ++i) {
          if (genStack[i].seen < genStack[i].gen.numChildren) {
            while (genStack[i].seen < genStack[i].gen.numChildren) {
              genStack[i].seen++;
              childFutures.push_back(createTask(childDepth + i, genStack[i].gen.next()));
            }
          }
        }
        backtracks = 0;
      }

      // If there's still children at this stackDepth we move into them
      if (genStack[stackDepth].seen < genStack[stackDepth].gen.numChildren) {
        genStack[stackDepth + 1].node = genStack[stackDepth].gen.next();
        const auto & child = genStack[stackDepth + 1].node;

        genStack[stackDepth].seen++;

        if constexpr(isDecision) {
          if (child.getObj() == reg->params.expectedObjective) {
            updateIncumbent<Space, Node, Bound, Objcmp, Verbose>(child, child.getObj());
            hpx::lcos::broadcast<SetStopFlagAct<Space, Node, Bound> >(hpx::find_all_localities());
            if constexpr (verbose >= 1) {
              hpx::cout <<
                  (boost::format("Found solution on: %1%\n")
                  % static_cast<std::int64_t>(hpx::get_locality_id()))
                        << hpx::flush;
            }
            return;
          }
        }

        // Do we support bounding?
        if constexpr(!std::is_same<boundFn, nullFn__>::value) {
          Objcmp cmp;
          auto bnd  = boundFn::invoke(space, child);
          if constexpr(isDecision) {
            if (!cmp(bnd, reg->params.expectedObjective) && bnd != reg->params.expectedObjective) {
                if constexpr(pruneLevel) {
                    stackDepth--;
                    depth--;
                    backtracks++;
                    continue;
                  } else {
                  continue;
                }
              }
            // B&B Case
            } else {
            auto best = reg->localBound.load();
            if (!cmp(bnd, best)) {
              if constexpr(pruneLevel) {
                  stackDepth--;
                  depth--;
                  backtracks++;
                  continue;
                } else {
                continue;
              }
            }
          }
        }

        if constexpr(isOptimisation) {
          // FIXME: unsure about loading this twice in terms of performance
          auto best = reg->localBound.load();
          Objcmp cmp;
          if (cmp(child.getObj(), best)) {
            updateIncumbent<Space, Node, Bound, Objcmp, Verbose>(child, child.getObj());
          }
        }

        // Going down
        const auto childGen = Generator(space, child);
        stackDepth++;
        depth++;

        if constexpr(isCountNodes) {
            counts[depth] += childGen.numChildren;
        }

        // TODO: This only works correctly for countNodes where we can count without going into a node
        // It wouldn't work for a depthBounded optimisation problem for example.
        if constexpr(isDepthBounded) {
          if (depth == reg->params.maxDepth) {
            stackDepth--;
            depth--;
            backtracks++;
            continue;
          }
        }

        genStack[stackDepth].seen = 0;
        genStack[stackDepth].gen = childGen;
      } else {
        stackDepth--;
        depth--;
        backtracks++;
      }
    }
  }

  static void subtreeTask(const Node taskRoot,
                          const unsigned childDepth,
                          const hpx::naming::id_type donePromiseId) {
    auto reg = Registry<Space, Node, Bound>::gReg;

    std::vector<std::uint64_t> countMap;
    if constexpr(isCountNodes) {
        countMap.resize(reg->params.maxDepth + 1);
    }

    std::vector<hpx::future<void> > childFutures;
    expand(reg->space, taskRoot, reg->params, countMap, childFutures, childDepth);

    // Atomically updates the (process) local counter
    if constexpr (isCountNodes) {
      reg->updateCounts(countMap);
    }

    hpx::apply(hpx::util::bind([=](std::vector<hpx::future<void> > & futs) {
          hpx::wait_all(futs);
          hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(donePromiseId, true);
        }, std::move(childFutures)));
  }

  static hpx::future<void> createTask(const unsigned childDepth,
                                      const Node & taskRoot) {
    hpx::lcos::promise<void> prom;
    auto pfut = prom.get_future();
    auto pid  = prom.get_id();

    detail::SubtreeTask<Generator, Args...> t;
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
    if constexpr (verbose) {
      printSkeletonDetails();
    }

    hpx::wait_all(hpx::lcos::broadcast<InitRegistryAct<Space, Node, Bound> >(
        hpx::find_all_localities(), space, root, params));

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

    createTask(1, root).get();

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
};

namespace detail {
template <typename Generator, typename ...Args>
struct SubtreeTask : hpx::actions::make_action<
  decltype(&Budget<Generator, Args...>::subtreeTask),
  &Budget<Generator, Args...>::subtreeTask,
  SubtreeTask<Generator, Args...>>::type {};

}

}}

#endif
