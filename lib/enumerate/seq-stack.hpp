#ifndef SKELETONS_ENUM_SEQSTACK_HPP
#define SKELETONS_ENUM_SEQSTACK_HPP

#include <cstdint>
#include <vector>
#include <tuple>

#include "nodegenerator.hpp"

#include <hpx/runtime/naming/address.hpp>

/* Like the sequential skeleton but without recursion */
namespace skeletons { namespace Enum { namespace SeqStack {

template <typename Space,
          typename Sol,
          typename Gen,
          std::size_t stackDepth>
void expand(const unsigned maxDepth,
            const Space & space,
            std::vector<std::uint64_t> & cntMap,
            const Sol & root) {
  auto depth = 0;

  // Setup the stack
  auto gen = Gen::invoke(0, space, root);

  using stackType = std::tuple<unsigned , Sol, decltype(gen)>;
  std::array<stackType, stackDepth> generatorStack;
  generatorStack[depth] = std::make_tuple(gen.numChildren, root, gen);

  cntMap[1] += std::get<2>(generatorStack[depth]).numChildren;

  // Don't enter the loop unless there is work to do
  if (maxDepth == 1) {
    return;
  }

  while (depth >= 0) {
    // No more children at this depth, backtrack
    if (std::get<0>(generatorStack[depth]) == 0) {
        depth--;
        continue;
    }

    // Ge the next child
    auto node = std::get<2>(generatorStack[depth]).next(space, std::get<1>(generatorStack[depth]));
    std::get<0>(generatorStack[depth])--;
    depth++;

    // Construct it's generator and count the children
    auto gen = Gen::invoke(0, space, node);
    cntMap[depth + 1] += gen.numChildren;

    // If we are deep enough then go back up the stack without pushing the generator
    if (maxDepth == depth + 1) {
      depth--;
      continue;
    }

    generatorStack[depth] = std::make_tuple(gen.numChildren, node, gen);
  }
}

template <typename Space,
          typename Sol,
          typename Gen,
          std::size_t stackDepth>
std::vector<std::uint64_t> count(const unsigned maxDepth,
                                 const Space & space,
                                 const Sol   & root) {
  std::vector<std::uint64_t> cntMap;
  cntMap.resize(maxDepth + 1);
  for (auto i = 1; i <= maxDepth; ++i) {
    cntMap[i] = 0;
  }
  cntMap[0] = 1; // Count root node

  expand<Space, Sol, Gen, stackDepth>(maxDepth, space, cntMap, root);

  return cntMap;
}

}}}

#endif
