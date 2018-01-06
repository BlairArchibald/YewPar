#ifndef SKELETONS_DEPTHSPAWN_HPP
#define SKELETONS_DEPTHSPAWN_HPP

#include <vector>
#include <cstdint>

#include "API.hpp"

#include "util/NodeGenerator.hpp"
#include "util/Registry.hpp"
#include "util/func.hpp"

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

  static bool seqExpand(const Space & space,
                        const Node & n,
                        const API::Params<Bound> & params,
                        Node & incumbent,
                        const unsigned childDepth,
                        std::vector<uint64_t> & counts) {
    Generator newCands = Generator(space, n);

    if constexpr(isCountNodes) {
        counts[childDepth] += newCands.numChildren;
    }

    if constexpr(isDepthBounded) {
        if (childDepth == params.maxDepth) {
          return false;
        }
      }

    for (auto i = 0; i < newCands.numChildren; ++i) {
      auto c = newCands.next();

      if constexpr(isDecision) {
        if (c.getObj() == params.expectedObjective) {
          incumbent = c;
          return true;
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
            auto best = incumbent.getObj();
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
        if (c.getObj() > incumbent.getObj()) {
          incumbent = c;
        }
      }

      auto found = seqExpand(space, c, params, incumbent, childDepth + 1, counts);
      if constexpr(isDecision) {
        // Propagate early exit
        if (found) {
          return true;
        }
      }
    }
    return false;
  }

  static Node doBnB (const Space & space,
                     const Node  & root,
                     const API::Params<Bound> & params) {
    auto incumbent = root;
    std::vector<std::uint64_t> counts(0);
    seqExpand(space, root, params, incumbent, 1, counts);
    return incumbent;
  }

  static std::vector<std::uint64_t> countNodes(const Space & space,
                                               const Node & root,
                                               const API::Params<Bound> & params) {
    auto incumbent = root;
    std::vector<std::uint64_t> counts(params.maxDepth + 1);
    counts[0] = 1;
    seqExpand(space, root, params, incumbent, 1, counts);
    return counts;
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

    // Create a main task and wait for it to finish

    hpx::wait_all(hpx::lcos::broadcast<Workstealing::Scheduler::stopSchedulers_act>(
        hpx::find_all_localities()));

    // Return the right thing
    if constexpr(isCountNodes) {
        return countNodes(space, root, params);
      } else if constexpr(isBnB || isDecision) {
        return doBnB(space, root, params);
      } else {
      static_assert(isCountNodes || isBnB || isDecision, "Please provide a supported search type: CountNodes, BnB, Decision");
    }
  }
};

}}

#endif
