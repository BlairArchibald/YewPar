#ifndef SKELETONS_COMMON_HPP
#define SKELETONS_COMMON_HPP

#include "util/Registry.hpp"
#include "util/Incumbent.hpp"

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

template<typename Space, typename Node, typename Bound>
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

}}

#endif
