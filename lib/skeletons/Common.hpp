#ifndef SKELETONS_COMMON_HPP
#define SKELETONS_COMMON_HPP

#include "util/Registry.hpp"
#include "util/Incumbent.hpp"
#include <numeric>

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

template <typename T>
static auto countDepths(std::vector<std::vector<T> > vec, const unsigned maxDepth) {
  std::vector<T> res(maxDepth + 1);
  for (auto i = 0; i <= maxDepth; ++i) {
    for (const auto & cnt : vec) {
      res[i] += cnt[i];
    }
  }
  
  return res;
}

template <typename Act>
static auto printMetric(const std::string && metric) {
  auto cntVec = hpx::lcos::broadcast<Act>(hpx::find_all_localities()).get();
  
  auto cnt = std::accumulate(cntVec.begin(), cntVec.end(), 0);
  hpx::cout << "Total number of " << metric << ": " << cnt << hpx::endl;
}

template <typename Space, typename Node, typename Bound>
static auto printNodeCounts() {
  printMetric<GetNodeCountAct<Space, Node, Bound> >("Nodes");
}

template <typename Space, typename Node, typename Bound>
static auto printPrunes() {
  printMetric<GetPrunesAct<Space, Node, Bound> >("Prunes");
}

template <typename Space, typename Node, typename Bound>
static auto printBacktracks() {
  printMetric<GetBacktracksAct<Space, Node, Bound> >("Backtracks");
}

template <typename Space, typename Node, typename Bound>
static auto printTimes(const unsigned maxDepth) {
  auto timesVecAll = hpx::lcos::broadcast<GetTimesAct<Space, Node, Bound> >(
      hpx::find_all_localities()).get();

  auto times = countDepths<double>(timesVecAll, maxDepth);
  times[0] = std::accumulate(times.begin(), times.end(), 0);
  for (int i = 0; i <= maxDepth; i++) {
    if (times[i] > 1) {
      hpx::cout << "Accumulated time at depth " << i << " " << times[i] << "ms" << hpx::endl;
    }
  }
}

template<typename Space, typename Node, typename Bound>
static std::vector<std::uint64_t> totalNodeCounts(const unsigned maxDepth) {
  auto cntList = hpx::lcos::broadcast<GetCountsAct<Space, Node, Bound> >(
      hpx::find_all_localities()).get();

  auto res = countDepths<std::uint64_t>(cntList, maxDepth);
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
