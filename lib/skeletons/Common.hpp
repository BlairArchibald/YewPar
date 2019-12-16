#ifndef SKELETONS_COMMON_HPP
#define SKELETONS_COMMON_HPP

#include "util/Registry.hpp"
#include "util/Incumbent.hpp"
#include "util/MetricStore.hpp"
#include <algorithm>
#include <cstdarg>
#include <functional>
#include <numeric>
#include <vector>

namespace YewPar { namespace Skeletons {

template<typename Space, typename Node, typename Bound, typename Cmp, typename Verbose>
static void initIncumbent(const Node & node, const Bound & bnd) {
  auto reg = Registry<Space, Node, Bound>::gReg;

  typedef typename Incumbent::InitComponentAct<Node, Bound, Cmp, Verbose> initComp;
  hpx::async<initComp>(reg->globalIncumbent).get();

  typedef typename Incumbent::InitialiseIncumbentAct<Node, Bound, Cmp, Verbose> initVals;
  hpx::async<initVals>(reg->globalIncumbent, node, bnd).get();
}

template<typename Space, typename Node, typename Bound, typename Cmp, typename Verbose>
static void updateIncumbent(const Node & node, const Bound & bnd) {
  auto reg = Registry<Space, Node, Bound>::gReg;

  (*reg).template updateRegistryBound<Cmp>(bnd);
  hpx::lcos::broadcast<UpdateRegistryBoundAct<Space, Node, Bound, Cmp> >(
      hpx::find_all_localities(), bnd);

  typedef typename Incumbent::UpdateIncumbentAct<Node, Bound, Cmp, Verbose> act;
  hpx::async<act>(reg->globalIncumbent, node).get();
}

template <typename Act>
static auto countDepths(const unsigned maxDepth, const std::string && metric="") {
  auto cntVecAll = hpx::lcos::broadcast<Act>(hpx::find_all_localities()).get();
  std::vector<std::uint64_t> res(maxDepth + 1); 

  for (int i = 0; i <= maxDepth; i++) {
    for (const auto & vec : cntVecAll) {
      if(vec[i] >= 1) {
        hpx::cout << "Metric " << metric << " Depth " << i << ": " << vec[i] << hpx::endl;
      }   
      res[i] += vec[i];
    }   
  }

  return res;
}

template <typename Act>
static auto printMetric(const std::string && metric, const unsigned maxDepth) {
  auto metricsVec = countDepths<Act>(maxDepth, std::move(metric));
}

static auto printNodeCounts(const unsigned maxDepth) {
  hpx::cout << "Node Counts" << hpx::endl;
  hpx::cout << "===========" << hpx::endl;
  printMetric<GetNodeCountAct>("Nodes", maxDepth);
  hpx::cout << "===========" << hpx::endl;
}

static auto printPrunes(const unsigned maxDepth) {
  hpx::cout << "Prunes" << hpx::endl;
  hpx::cout << "===========" << hpx::endl;
  printMetric<GetPrunesAct>("Prunes", maxDepth);
  hpx::cout << "===========" << hpx::endl;
}

static auto printBacktracks(const unsigned maxDepth) {
  hpx::cout << "Backtracks" << hpx::endl;
  hpx::cout << "===========" << hpx::endl;
  printMetric<GetBacktracksAct>("Backtracks", maxDepth);
  hpx::cout << "===========" << hpx::endl;
}

static auto printTimes(const unsigned maxDepth) {

  auto timeVec = countDepths<GetAccumulatedTimesAct>(maxDepth);

  auto timeBucketsAll = hpx::lcos::broadcast<GetTimeBucketsAct>(hpx::find_all_localities()).get();

  std::vector<std::vector<std::uint64_t> > timeBuckets(maxDepth + 1, std::vector<std::uint64_t>(13));
  std::uint64_t depth = 0;

  for (std::vector<std::vector<std::uint64_t> > bucketsLocality : timeBucketsAll) {
    for (int i = 0; i <= maxDepth; i++) {
      for (const auto & bucket : bucketsLocality[i]) {
        if (bucket >= 1) {
          hpx::cout << "Depth " << i << " Bucket " << depth++ << ": " << bucket << hpx::endl;
        }
				depth = 0;
      }
    }
  }

}

template <typename Space, typename Node, typename Bound>
static std::vector<std::uint64_t> totalNodeCounts(const unsigned maxDepth) {
  auto res = countDepths<GetCountsAct<Space, Node, Bound> >(maxDepth);

  res[0] = 1; //Account for root node
  return res;
}

template <typename Generator>
struct StackElem {
  unsigned seen;
  typename Generator::Nodetype node;
  Generator gen;

  StackElem(Generator gen) : seen(0), gen(gen) {};
  StackElem(const typename Generator::Spacetype & s,
            const typename Generator::Nodetype & n)
      : seen(0), node(n), gen(Generator(s, node)) {};
};

template <typename Generator>
using GeneratorStack = std::vector<StackElem<Generator>>;

// General node processing
enum ProcessNodeRet { Exit, Prune, Break, Continue };

template <typename Space, typename Node, typename ...Args>
struct ProcessNode {
  typedef typename API::skeleton_signature::bind<Args...>::type args;

  static constexpr bool isOptimisation = parameter::value_type<args, API::tag::Optimisation_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool isDecision = parameter::value_type<args, API::tag::Decision_, std::integral_constant<bool, false> >::type::value;
  static constexpr bool pruneLevel = parameter::value_type<args, API::tag::PruneLevel_, std::integral_constant<bool, false> >::type::value;

  typedef typename parameter::value_type<args, API::tag::Verbose_, std::integral_constant<unsigned, 0> >::type Verbose;

  typedef typename parameter::value_type<args, API::tag::BoundFunction, nullFn__>::type boundFn;
  typedef typename boundFn::return_type Bound;
  typedef typename parameter::value_type<args, API::tag::ObjectiveComparison, std::greater<Bound> >::type Objcmp;

  static ProcessNodeRet processNode(const API::Params<Bound> & params,
                                    const Space & space,
                                    const Node & c) {
    if constexpr(isDecision) {
        if (c.getObj() == params.expectedObjective) {
          updateIncumbent<Space, Node, Bound, Objcmp, Verbose>(c, c.getObj());
          hpx::lcos::broadcast<SetStopFlagAct<Space, Node, Bound> >(hpx::find_all_localities());
          return ProcessNodeRet::Exit;
        }
      }

    if constexpr(!std::is_same<boundFn, nullFn__>::value) {
        Objcmp cmp;
        auto bnd  = boundFn::invoke(space, c);
        if constexpr(isDecision) {
            if (!cmp(bnd, params.expectedObjective) && bnd != params.expectedObjective) {
              if constexpr(pruneLevel) {
                  return ProcessNodeRet::Break;
                } else {
                return ProcessNodeRet::Prune;
              }
            }
            // B&B Case
          } else {
          auto reg = Registry<Space, Node, Bound>::gReg;
          auto best = reg->localBound.load();
          if (!cmp(bnd, best)) {
            if constexpr(pruneLevel) {
                return ProcessNodeRet::Break;
            } else {
              return ProcessNodeRet::Prune;
            }
          }
        }
      }

    if constexpr(isOptimisation) {
        auto reg = Registry<Space, Node, Bound>::gReg;
        auto best = reg->localBound.load();

        Objcmp cmp;
        if (cmp(c.getObj(),best)) {
          updateIncumbent<Space, Node, Bound, Objcmp, Verbose>(c, c.getObj());
        }
    }
    return ProcessNodeRet::Continue;
  }
};

}}

#endif
