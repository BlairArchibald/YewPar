#ifndef SKELETONS_ENUM_SEQ_HPP
#define SKELETONS_ENUM_SEQ_HPP

#include <cstdint>
#include <vector>
#include <tuple>

namespace skeletons { namespace Enum {

// Supported traversal methods
struct Rec {};
struct Stack {};

template <typename Space, typename Sol, typename Gen, typename ...Args>
struct Count {};

template <typename Space, typename Sol, typename Gen>
struct Count<Space, Sol, Gen, Rec> {
  static void expand(const unsigned maxDepth,
              const Space & space,
              std::vector<std::uint64_t> & cntMap,
              const unsigned childDepth,
              const Sol & n) {
    auto newCands = Gen::invoke(space, n);

    cntMap[childDepth] += newCands.numChildren;

    if (maxDepth == childDepth) {
      return;
    }

    for (auto i = 0; i < newCands.numChildren; ++i) {
        auto c = newCands.next();
        expand(maxDepth, space, cntMap, childDepth + 1, c);
      }
  }

  static std::vector<std::uint64_t> search(const unsigned maxDepth,
                                    const Space & space,
                                    const Sol   & root) {
    std::vector<std::uint64_t> cntMap;
    cntMap.resize(maxDepth + 1);
    for (auto i = 1; i <= maxDepth; ++i) {
      cntMap[i] = 0;
    }
    cntMap[0] = 1; // Count root node

    expand(maxDepth, space, cntMap, 1, root);

    return cntMap;
  }
};

//template <typename Space, typename Sol, typename Gen, std::size_t maxStackDepth_>
template <typename Space, typename Sol, typename Gen, std::size_t maxStackDepth_>
struct Count<Space, Sol, Gen, Stack, std::integral_constant<std::size_t, maxStackDepth_>>{
  static void expand(const unsigned maxDepth,
                     const Space & space,
                     std::vector<std::uint64_t> & cntMap,
                     const Sol & root) {
    auto depth = 0;

    // Setup the stack
    auto gen = Gen::invoke(space, root);

    using stackType = std::tuple<unsigned, decltype(gen)>;
    std::array<stackType, maxStackDepth_> generatorStack;
    generatorStack[depth] = std::make_tuple(gen.numChildren, gen);

    cntMap[1] += std::get<1>(generatorStack[depth]).numChildren;

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
      auto node = std::get<1>(generatorStack[depth]).next();
      std::get<0>(generatorStack[depth])--;
      depth++;

      // Construct it's generator and count the children
      auto gen = Gen::invoke(space, node);
      cntMap[depth + 1] += gen.numChildren;

      // If we are deep enough then go back up the stack without pushing the generator
      if (maxDepth == depth + 1) {
        depth--;
        continue;
      }

      generatorStack[depth] = std::make_tuple(gen.numChildren, std::move(gen));
    }
  }

  static std::vector<std::uint64_t> search(const unsigned maxDepth,
                                          const Space & space,
                                          const Sol   & root) {
    std::vector<std::uint64_t> cntMap;
    cntMap.resize(maxDepth + 1);
    for (auto i = 1; i <= maxDepth; ++i) {
      cntMap[i] = 0;
    }
    cntMap[0] = 1; // Count root node

    expand(maxDepth, space, cntMap, root);

    return cntMap;
  }
};


// We default to the recursive version
template <typename Space, typename Sol, typename Gen>
struct Count<Space, Sol, Gen> : Count<Space, Sol, Gen, Rec> {};

}}

#endif
