#ifndef SKELETONS_SEQ_HPP
#define SKELETONS_SEQ_HPP

#include <hpx/include/iostreams.hpp>
#include <vector>
#include <cstdint>

#include <boost/format.hpp>

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
  static constexpr bool isBnB = parameter::value_type<args, API::tag::Optimisation_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isDecision = parameter::value_type<args, API::tag::Decision_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isDepthBounded = parameter::value_type<args, API::tag::DepthLimited_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool pruneLevel = parameter::value_type<args, API::tag::PruneLevel_, std::integral_constant<bool, false> >::type::value;
  static constexpr unsigned verbose = parameter::value_type<args, API::tag::Verbose_, std::integral_constant<unsigned, 0> >::type::value;
  typedef typename parameter::value_type<args, API::tag::BoundFunction, nullFn__>::type boundFn;
  typedef typename boundFn::return_type Bound;
  typedef typename parameter::value_type<args, API::tag::ObjectiveComparison, std::greater<Bound> >::type Objcmp;

  static void printSkeletonDetails() {
    hpx::cout << "Skeleton Type: Seq\n";
    hpx::cout << "CountNodes : " << std::boolalpha << isCountNodes << "\n";
    hpx::cout << "Optimisation: " << std::boolalpha << isBnB << "\n";
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

  static bool expand(const Space & space,
                     const Node & n,
                     const API::Params<Bound> & params,
                     std::pair<Node, Bound> & incumbent,
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
          std::get<0>(incumbent) = c;
          if constexpr(verbose > 1) {
            hpx::cout <<
              (boost::format("Found solution on: %1%\n")
              % static_cast<std::int64_t>(hpx::get_locality_id()))
                      << hpx::flush;
          }
          return true;
        }
      }

      // Do we support bounding?
      if constexpr(!std::is_same<boundFn, nullFn__>::value) {
          Objcmp cmp;
          auto bnd  = boundFn::invoke(space, c);
          if constexpr(isDecision) {
            if (!cmp(bnd, params.expectedObjective) && bnd != params.expectedObjective) {
              if constexpr(pruneLevel) {
                break;
              } else {
                continue;
              }
            }
          // B&B Case
          } else {
            auto best = std::get<1>(incumbent);
            if (!cmp(bnd,best)) {
              if constexpr(pruneLevel) {
                  break;
                } else {
                continue;
              }
            }
        }
      }

      if constexpr(isBnB) {
        Objcmp cmp;
        if (cmp(c.getObj(), std::get<1>(incumbent))) {
          std::get<0>(incumbent) = c;
          std::get<1>(incumbent) = c.getObj();
          if constexpr(verbose >= 1) {
            hpx::cout << (boost::format("New Incumbent: %1%\n") % c.getObj()) << hpx::flush;
          }
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

  static auto search (const Space & space,
                      const Node & root,
                      const API::Params<Bound> params = API::Params<Bound>()) {
    static_assert(isCountNodes || isBnB || isDecision, "Please provide a supported search type: CountNodes, BnB, Decision");

    if constexpr (verbose) {
      printSkeletonDetails();
    }

    std::vector<std::uint64_t> counts;

    if constexpr(isCountNodes) {
      counts.resize(params.maxDepth + 1);
      counts[0] = 1;
    }

    std::pair<Node, Bound> incumbent = std::make_pair(root, params.initialBound);
    expand(space, root, params, incumbent, 1, counts);

    if constexpr(isCountNodes && (isBnB || isDecision)) {
        return std::make_pair(std::get<0>(incumbent), counts);
    } else if constexpr(isCountNodes) {
      return counts;
    } else {
      return std::get<0>(incumbent);
    }
  }
};


}}

#endif
