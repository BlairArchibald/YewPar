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
  static constexpr bool isDepthBounded = parameter::value_type<args, API::tag::DepthBounded_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool pruneLevel = parameter::value_type<args, API::tag::PruneLevel_, std::integral_constant<bool, false> >::type::value;
  typedef typename parameter::value_type<args, API::tag::BoundFunction, nullFn__>::type boundFn;

  static void expandBnB (const Space & space,
                         const Node  & n,
                         Node & incumbent) {
    Generator newCands = Generator(space, n);

    for (auto i = 0; i < newCands.numChildren; ++i) {
      auto c = newCands.next();

      // Do we support bounding
      if constexpr(!std::is_same<boundFn, nullFn__>::value) {
          auto bnd  = boundFn::invoke(space, c);
          auto best = incumbent.getObj();
          // TODO: Pass bound cmp function
          if (bnd <= best) {
            if constexpr(pruneLevel) {
              break;
            } else {
              continue;
            }
          }
      }

      // TODO: Allow only updating when there are no children left?
      if (c.getObj() > incumbent.getObj()) {
        incumbent = c;
      }

      expandBnB(space, c, incumbent);
    }
  }

  static Node doBnB (const Space & space,
                     const Node  & root) {
    auto incumbent = root;
    expandBnB(space, root, incumbent);
    return incumbent;
  }

  // TODO: Have a single function that always "counts" but returns an empty vector if we toggle it off at compile time (then we return the right thing in the "search" dispatcher)
  static void expandCounts (const unsigned maxDepth,
                            const Space & space,
                            const Node & n,
                            const unsigned childDepth,
                            std::vector<std::uint64_t> & counts) {
    Generator newCands;
    if constexpr(std::is_same<Space, Empty>::value) {
      newCands = Generator(n);
    } else {
      newCands = Generator(space, n);
    }

    counts[childDepth] += newCands.numChildren;

    if constexpr(isDepthBounded) {
        if (childDepth == maxDepth) {
          return;
        }
    }

    for (auto i = 0; i < newCands.numChildren; ++i) {
      auto c = newCands.next();
      expandCounts(maxDepth, space, c, childDepth + 1, counts);
    }
  }

  static std::vector<std::uint64_t> countNodes(const unsigned maxDepth,
                                               const Space & space,
                                               const Node & root) {
    // TODO: How to specify maxDepth in an easy way (ideally not at compile time? Or maybe this is okay?)
    std::vector<std::uint64_t> counts(maxDepth + 1);

    counts[0] = 1;

    expandCounts(maxDepth, space, root, 1, counts);

    return counts;
  }

  static auto search (const Space & space,
                      const Node & root,
                      const API::Params<Bound> params = API::Params<Bound>()) {
    if constexpr(isCountNodes) {
      return countNodes(params.maxDepth, space, root);
    } else if constexpr(isBnB) {
      return doBnB(space, root);
    } else {
    }
  }
};


}}

#endif
