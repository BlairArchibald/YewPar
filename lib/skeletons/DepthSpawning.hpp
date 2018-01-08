#ifndef SKELETONS_DEPTHSPAWN_HPP
#define SKELETONS_DEPTHSPAWN_HPP

#include <vector>
#include <cstdint>

#include "API.hpp"

#include <hpx/lcos/broadcast.hpp>

#include "util/NodeGenerator.hpp"
#include "util/Registry.hpp"
#include "util/Incumbent.hpp"
#include "util/func.hpp"
#include "util/doubleWritePromise.hpp"

#include "workstealing/Scheduler.hpp"
#include "workstealing/policies/Workpool.hpp"

namespace YewPar { namespace Skeletons {

// This skeleton allows spawning all tasks into a workqueue based policy based on some depth limit
// TODO: Not sure on the name for this
template <typename Generator, typename ...Args>
struct DepthSpawns {
  typedef typename Generator::Nodetype Node;
  typedef typename Generator::Spacetype Space;

  typedef typename API::skeleton_signature::bind<Args...>::type args;

  static constexpr bool isCountNodes = parameter::value_type<args, API::tag::CountNodes_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isBnB = parameter::value_type<args, API::tag::BnB_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isDecision = parameter::value_type<args, API::tag::Decision_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isDepthBounded = parameter::value_type<args, API::tag::DepthBounded_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool pruneLevel = parameter::value_type<args, API::tag::PruneLevel_, std::integral_constant<bool, false> >::type::value;
  typedef typename parameter::value_type<args, API::tag::BoundFunction, nullFn__>::type boundFn;
  typedef typename boundFn::return_type Bound;

  static void updateIncumbent(const Node & node, const Bound & bnd) {
    auto reg = Registry<Space, Node, Bound>::gReg;
    // Should we force this local update for performance?
    //reg->updateRegistryBound(bnd)
    hpx::lcos::broadcast<UpdateRegistryBoundAct<Space, Node, Bound> >(
        hpx::find_all_localities(), bnd);

    typedef typename Incumbent<Node>::UpdateIncumbentAct act;
    hpx::async<act>(reg->globalIncumbent, node).get();
  }

  static void expandWithSpawns(const Space & space,
                               const Node & n,
                               const API::Params<Bound> & params,
                               std::vector<uint64_t> & counts,
                               std::vector<hpx::future<void> > & childFutures,
                               const unsigned childDepth) {
    auto reg = Registry<Space, Node, Bound>::gReg;
    Generator newCands = Generator(space, n);

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

      if constexpr(isDecision) {
        if (c.getObj() == params.expectedObjective) {
          updateIncumbent(c, c.getObj());
          return;
        }
      }

      // Do we support bounding?
      if constexpr(!std::is_same<boundFn, nullFn__>::value) {
          auto bnd  = boundFn::invoke(space, c);
          if constexpr(isDecision) {
            if (bnd < params.expectedObjective) {
              if constexpr(pruneLevel) {
                  break;
                } else {
                continue;
              }
            }
          // B&B Case
          } else {
            auto best = reg->localBound.load();
            if (bnd <= best) {
              if constexpr(pruneLevel) {
                  break;
                } else {
                continue;
              }
            }
        }
      }

      if constexpr(isBnB) {
        // FIXME: unsure about loading this twice in terms of performance
        auto best = reg->localBound.load();

        if (c.getObj() > best) {
          updateIncumbent(c, c.getObj());
        }
      }

      // Spawn new tasks for all children (that are still alive after pruning)
      childFutures.push_back(createTask(childDepth + 1, c));
    }
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

      if constexpr(isDecision) {
        if (c.getObj() == params.expectedObjective) {
          updateIncumbent(c, c.getObj());
          // Found a solution, tell the main thread to kill everything
          hpx::async<util::DoubleWritePromise<bool>::set_value_action>(reg->foundPromiseId, true).get();
          return;
        }
      }

      // Do we support bounding?
      if constexpr(!std::is_same<boundFn, nullFn__>::value) {
          auto bnd  = boundFn::invoke(space, c);
          if constexpr(isDecision) {
            if (bnd < params.expectedObjective) {
              if constexpr(pruneLevel) {
                  break;
                } else {
                continue;
              }
            }
          // B&B Case
          } else {
            auto best = reg->localBound.load();
            if (bnd <= best) {
              if constexpr(pruneLevel) {
                  break;
                } else {
                continue;
              }
            }
        }
      }

      if constexpr(isBnB) {
        // FIXME: unsure about loading this twice in terms of performance
        auto best = reg->localBound.load();
        if (c.getObj() > best) {
          updateIncumbent(c, c.getObj());
        }
      }

      expandNoSpawns(space, c, params, counts, childDepth + 1);
    }
  }

  static void subtreeTask(const Node taskRoot,
                          const unsigned childDepth,
                          const hpx::naming::id_type donePromiseId,
                          const bool isMainTask = false) {
    auto reg = Registry<Space, Node, Bound>::gReg;

    std::vector<std::uint64_t> countMap;
    if constexpr(isCountNodes) {
        countMap.resize(reg->params.maxDepth + 1);
    }

    std::vector<hpx::future<void> > childFutures;

    if (childDepth <= reg->params.spawnDepth) {
      expandWithSpawns(reg->space, taskRoot, reg->params, countMap, childFutures, childDepth);
    } else {
      expandNoSpawns(reg->space, taskRoot, reg->params, countMap, childDepth);
    }

    // Atomically updates the (process) local counter
    if constexpr (isCountNodes) {
      reg->updateCounts(countMap);
    }

    hpx::apply(hpx::util::bind([=](std::vector<hpx::future<void> > & futs) {
          hpx::wait_all(futs);
          hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(donePromiseId, true);
          if (isMainTask) {
            hpx::async<util::DoubleWritePromise<bool>::set_value_action>(reg->foundPromiseId, true).get();
          }
        }, std::move(childFutures)));
  }

  struct SubtreeTask : hpx::actions::make_action<
    decltype(&DepthSpawns<Generator, Args...>::subtreeTask),
    &DepthSpawns<Generator, Args...>::subtreeTask,
    SubtreeTask>::type {};

  static hpx::future<void> createTask(const unsigned childDepth,
                                      const Node & taskRoot,
                                      const bool isMainTask = false) {
    hpx::lcos::promise<void> prom;
    auto pfut = prom.get_future();
    auto pid  = prom.get_id();

    SubtreeTask t;
    hpx::util::function<void(hpx::naming::id_type)> task;
    task = hpx::util::bind(t, hpx::util::placeholders::_1, taskRoot, childDepth, pid, isMainTask);

    // TODO: Type alias this stuff
    auto workPool = std::static_pointer_cast<Workstealing::Policies::Workpool>(Workstealing::Scheduler::local_policy);
    workPool->addwork(task);

     return pfut;
  }

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

    Workstealing::Policies::Workpool::initPolicy();

    auto threadCount = hpx::get_os_thread_count() == 1 ? 1 : hpx::get_os_thread_count() - 1;
    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::startSchedulers_act>(
        hpx::find_all_localities(), threadCount));

    if constexpr(isBnB || isDecision) {
      auto inc = hpx::new_<Incumbent<Node> >(hpx::find_here()).get();
      hpx::wait_all(hpx::lcos::broadcast<UpdateGlobalIncumbentAct<Space, Node, Bound> >(
          hpx::find_all_localities(), inc));
      updateIncumbent(root, root.getObj());
    }

    // Issue is updateCounts by the looks of things. Something probably isn't initialised correctly.
    if constexpr(isDecision) {
      hpx::promise<int> foundProm;
      auto foundFut = foundProm.get_future();
      auto foundId = hpx::new_<util::DoubleWritePromise<bool> >(hpx::find_here(), foundProm.get_id()).get();
      hpx::wait_all(hpx::lcos::broadcast<SetFoundPromiseIdAct<Space, Node, Bound> >(
          hpx::find_all_localities(), foundId));
      createTask(1, root, true);
      foundFut.get(); // Allows early termination
    } else {
      createTask(1, root).get();
    }

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
