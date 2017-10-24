#ifndef ENUM_REGISTRY_HPP
#define ENUM_REGISTRY_HPP

#include <vector>

namespace skeletons { namespace Enum {

template <typename Space, typename Sol>
struct Registry {
  static Registry<Space, Sol>* gReg;

  Space space_;

  unsigned maxDepth;
  std::vector<std::atomic<std::uint64_t> >* counts; //TODO: Why is this a pointer?

  // For recompute we need to store the root
  Sol root_;

  void updateCounts(std::vector<std::uint64_t> & cntMap) {
    for (auto i = 0; i <= maxDepth; ++i) {
      // Addition happens atomically
      (*counts)[i] += cntMap[i];
    }
  }
};

template<typename Space, typename Sol>
Registry<Space, Sol>* Registry<Space, Sol>::gReg = new Registry<Space, Sol>;

template <typename Space, typename Sol>
void initialiseRegistry(Space space, unsigned maxDepth , Sol root) {
  auto reg = Registry<Space, Sol>::gReg;
  reg->space_ = space;
  reg->maxDepth = maxDepth;
  reg->root_ = root;

  // FIXME: Technically a memory leak
  // FIXME: Should we just move to fixed size arrays for this?
  reg->counts = new std::vector<std::atomic<std::uint64_t>>(maxDepth + 1);
  for (auto i = 0; i <= reg->maxDepth; ++i) {
    (*reg->counts)[i] = 0;
  }
}

template <typename Space, typename Sol>
std::vector<std::uint64_t> getCounts() {
  auto reg = Registry<Space, Sol>::gReg;
  std::vector<std::uint64_t> res;

  // Convert std::atomic<std::uint64_t> -> uint64_t by loading it
  res.resize(reg->maxDepth + 1);
  for (auto i = 0; i <= reg->maxDepth; ++i) {
    res[i] = (*reg->counts)[i].load();
  }

  return res;
}

template<typename Space, typename Sol>
struct EnumGetCountsAct : hpx::actions::make_action<
  decltype(&getCounts<Space, Sol>), &getCounts<Space, Sol>, EnumGetCountsAct<Space, Sol> >::type {};

template<typename Space, typename Sol>
struct EnumInitRegistryAct : hpx::actions::make_action<
  decltype(&initialiseRegistry<Space, Sol>), &initialiseRegistry<Space, Sol>, EnumInitRegistryAct<Space, Sol> >::type {};

}}

#endif
