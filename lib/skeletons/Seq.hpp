#ifndef SKELETONS_SEQ_HPP
#define SKELETONS_SEQ_HPP

#include <vector>
#include <cstdint>

#include "API.hpp"
#include "util/NodeGenerator.hpp"
#include "util/func.hpp"

namespace YewPar { namespace Skeletons {

template <typename Generator, typename ...Args>
struct Seq {
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

  static bool expand(const Space & space,
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

      auto found = expand(space, c, params, incumbent, childDepth + 1, counts);
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
    expand(space, root, params, incumbent, 1, counts);
    return incumbent;
  }

  static std::vector<std::uint64_t> countNodes(const Space & space,
                                               const Node & root,
                                               const API::Params<Bound> & params) {
    auto incumbent = root;
    std::vector<std::uint64_t> counts(params.maxDepth + 1);
    counts[0] = 1;
    expand(space, root, params, incumbent, 1, counts);
    return counts;
  }

  static auto search (const Space & space,
                      const Node & root,
                      const API::Params<Bound> params = API::Params<Bound>()) {
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
