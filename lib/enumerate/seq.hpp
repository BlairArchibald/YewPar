#ifndef SKELETONS_ENUM_SEQ_HPP
#define SKELETONS_ENUM_SEQ_HPP

#include <cstdint>
#include <vector>

namespace skeletons { namespace Enum { namespace Seq {

template <typename Space,
          typename Sol,
          typename Gen>
void expand(const unsigned maxDepth,
            const Space & space,
            std::vector<std::uint64_t> & cntMap,
            const unsigned childDepth,
            const Sol & n) {
  auto newCands = Gen::invoke(0, space, n);

  cntMap[childDepth] += newCands.numChildren;

  if (maxDepth == childDepth) {
    return;
  }

  for (auto i = 0; i < newCands.numChildren; ++i) {
      auto c = newCands.next(space, n);
      expand<Space, Sol, Gen>(maxDepth, space, cntMap, childDepth + 1, c);
    }
}

template <typename Space,
          typename Sol,
          typename Gen>
std::vector<std::uint64_t> count(const unsigned maxDepth,
                                 const Space & space,
                                 const Sol   & root) {
  std::vector<std::uint64_t> cntMap;
  cntMap.resize(maxDepth);
  for (auto i = 1; i <= maxDepth; ++i) {
    cntMap[i] = 0;
  }
  cntMap[0] = 1; // Count root node

  expand<Space, Sol, Gen>(maxDepth, space, cntMap, 1, root);

  return cntMap;
}

}}}

#endif
