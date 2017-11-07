#ifndef ENUM_REGISTRY_HPP
#define ENUM_REGISTRY_HPP

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iterator>
#include <memory>
#include <vector>

namespace skeletons { namespace Enum {

template <typename Space, typename Sol>
struct Registry {
  static Registry<Space, Sol>* gReg;

  Space space;
  Sol root;
  unsigned maxDepth;

  using countMapT = std::vector<std::atomic<std::uint64_t> >;
  std::unique_ptr<countMapT> counts;

  void updateCounts(std::vector<std::uint64_t> & cntMap) {
    for (auto i = 0; i <= maxDepth; ++i) {
      // Addition happens atomically
      (*counts)[i] += cntMap[i];
    }
  }

  void initialise(Space space, unsigned maxDepth, Sol root) {
    this->space = space;
    this->maxDepth = maxDepth;
    this->root = root;
    this->counts = std::make_unique<countMapT>(maxDepth + 1);
  }

  std::vector<std::uint64_t> getCounts() {
    // Convert std::atomic<std::uint64_t> -> uint64_t by loading it
    std::vector<std::uint64_t> res;
    std::transform(counts->begin(), counts->end(), std::back_inserter(res), [](const auto & c) { return c.load(); });
    return res;
  }
};

template<typename Space, typename Sol>
Registry<Space, Sol>* Registry<Space, Sol>::gReg = new Registry<Space, Sol>;

template <typename Space, typename Sol>
void initialiseRegistry(Space space, unsigned maxDepth , Sol root) {
  Registry<Space, Sol>::gReg->initialise(space, maxDepth, root);
}

template <typename Space, typename Sol>
std::vector<std::uint64_t> getCounts() {
  return Registry<Space, Sol>::gReg->getCounts();
}

template<typename Space, typename Sol>
struct EnumGetCountsAct : hpx::actions::make_action<
  decltype(&getCounts<Space, Sol>), &getCounts<Space, Sol>, EnumGetCountsAct<Space, Sol> >::type {};

template<typename Space, typename Sol>
struct EnumInitRegistryAct : hpx::actions::make_action<
  decltype(&initialiseRegistry<Space, Sol>), &initialiseRegistry<Space, Sol>, EnumInitRegistryAct<Space, Sol> >::type {};

}}

#endif
