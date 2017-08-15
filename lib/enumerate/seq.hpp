#ifndef SKELETONS_ENUM_SEQ_HPP
#define SKELETONS_ENUM_SEQ_HPP

#include <cstdint>
#include <map>

namespace skeletons { namespace Enum { namespace Seq {

template <typename Space,
          typename Sol,
          typename Gen>
void expand(const unsigned maxDepth,
            const Space & space,
            std::map<unsigned, std::uint64_t> & cntMap,
            const unsigned childDepth,
            const Sol & n) {
  auto newCands = Gen::invoke(0, space, n);

  if (cntMap[childDepth]) {
    cntMap[childDepth] += newCands.numChildren;
  } else {
    cntMap[childDepth] = newCands.numChildren;
  }

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
std::map<unsigned, std::uint64_t>
count(const unsigned maxDepth,
      const Space & space,
      const Sol   & root) {
  std::map<unsigned, uint64_t> cntMap;
  cntMap[0] = 1; // Count root node

  expand<Space, Sol, Gen>(maxDepth, space, cntMap, 1, root);

  return cntMap;
}

}}}

#endif
