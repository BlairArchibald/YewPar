#ifndef SKELETONS_ENUM_UTIL_HPP
#define SKELETONS_ENUM_UTIL_HPP

#include <vector>
#include <cstdint>
#include "enumRegistry.hpp"

namespace skeletons { namespace Enum {

template <typename Space, typename Sol>
std::vector<std::uint64_t> totalNodeCounts(const unsigned maxDepth) {
  auto cntList = hpx::lcos::broadcast<EnumGetCountsAct<Space, Sol> >(hpx::find_all_localities()).get();
  std::vector<std::uint64_t> res(maxDepth + 1);
  for (auto i = 0; i <= maxDepth; ++i) {
    for (const auto & cnt : cntList) {
      res[i] += cnt[i];
    }
  }
  res[0] = 1; //Account for root node
  return res;
}

}}

#endif
