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
    std::vector<std::uint64_t> cntMap(maxDepth + 1);

    cntMap[0] = 1; // Count root node

    expand(maxDepth, space, cntMap, 1, root);

    return cntMap;
  }
};

//template <typename Space, typename Sol, typename Gen, std::size_t maxStackDepth_>
template <typename Space, typename Sol, typename Gen, std::size_t maxStackDepth_>
struct Count<Space, Sol, Gen, Stack, std::integral_constant<std::size_t, maxStackDepth_>>{
  struct StackElem {
    unsigned seen;
    typename Gen::return_type generator;
  };

  static void expand(const unsigned maxDepth,
                     const Space & space,
                     std::vector<std::uint64_t> & cntMap,
                     const Sol & root) {
    std::array<StackElem, maxStackDepth_> generatorStack;

    // Setup the stack with root node
    auto rootGen = Gen::invoke(space, root);
    cntMap[1] += rootGen.numChildren;
    generatorStack[0].seen = 0;
    generatorStack[0].generator = std::move(rootGen);

    int depth = 0;
    while (depth >= 0) {
      // If there's still children at this depth we move into them
      if (generatorStack[depth].seen < generatorStack[depth].generator.numChildren) {
        // Get the next child at this depth
        const auto child = generatorStack[depth].generator.next();
        generatorStack[depth].seen++;

        // Going down
        depth++;

        // Get the child's generator
        const auto childGen = Gen::invoke(space, child);
        cntMap[depth + 1] += childGen.numChildren;

        // We don't even need to store the generator if we are just counting the children
        if (depth + 1 == maxDepth) {
          depth--;
        } else {
          generatorStack[depth].seen = 0;
          generatorStack[depth].generator = std::move(childGen);
        }
      } else {
        depth--;
      }
    }
  }

  static std::vector<std::uint64_t> search(const unsigned maxDepth,
                                          const Space & space,
                                          const Sol   & root) {
    std::vector<std::uint64_t> cntMap(maxDepth + 1);

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
