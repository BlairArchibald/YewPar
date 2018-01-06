#ifndef YEWPAR_REGISTRY_HPP
#define YEWPAR_REGISTRY_HPP

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iterator>
#include <memory>
#include <vector>

#include "skeletons/API.hpp"

namespace YewPar {

template <typename Space, typename Node, typename Bound>
struct Registry {
  static Registry<Space, Node, Bound>* gReg;

  // General parameters
  Space space;
  Node root;

  Skeletons::API::Params<Bound> params;

  // BNB
  std::atomic<Bound> localBound;
  hpx::naming::id_type globalIncumbent;

  // Decision problems
  std::atomic<bool> stopSearch {false};

  // Coounting Nodes
  using countMapT = std::vector<std::atomic<std::uint64_t> >;
  std::unique_ptr<std::vector<std::atomic<std::uint64_t> > > counts;

  // We construct this object globally at compile time (see below) so this can't
  // happen in the constructor and should instead be called as an action on each
  // locality.
  void initialise(Space space, Node root, Skeletons::API::Params<Bound> params) {
    this->space = space;
    this->root = root;
    this->params = params;
  }

  // Counting
  void updateCounts(std::vector<std::uint64_t> & cntMap) {
    for (auto i = 0; i <= params.maxDepth ; ++i) {
      // Addition happens atomically
      (*counts)[i] += cntMap[i];
    }
  }

  std::vector<std::uint64_t> getCounts() {
    // Convert std::atomic<std::uint64_t> -> uint64_t by loading it
    std::vector<std::uint64_t> res;
    std::transform(counts->begin(), counts->end(), std::back_inserter(res), [](const auto & c) { return c.load(); });
    return res;
  }

  // BNB
  void updateRegistryBound(Bound bnd) {
    while (true) {
      auto curBound = localBound.load();
      if (bnd < curBound) {
        break;
      }

      if (localBound.compare_exchange_weak(curBound, bnd)) {
        break;
      }
    }
  }

  void setStopSearchFlag() {
    stopSearch.store(true);
  }

};

template<typename Space, typename Node, typename Bound>
Registry<Space, Node, Bound>* Registry<Space, Node, Bound>::gReg = new Registry<Space, Node, Bound>;

// Easy calling
template <typename Space, typename Node, typename Bound>
void initialiseRegistry(Space space, Node root, YewPar::Skeletons::API::Params<Bound> params) {
  Registry<Space, Node, Bound>::gReg->initialise(space, root, params);
}
template <typename Space, typename Node, typename Bound>
struct InitRegistryAct : hpx::actions::make_action<
  decltype(&initialiseRegistry<Space, Node, Bound>), &initialiseRegistry<Space, Node, Bound>, InitRegistryAct<Space, Node, Bound> >::type {};

template <typename Space, typename Node, typename Bound>
std::vector<std::uint64_t> getCounts() {
  return Registry<Space, Node, Bound>::gReg->getCounts();
}
template <typename Space, typename Node, typename Bound>
struct GetCountsAct : hpx::actions::make_action<
  decltype(&getCounts<Space, Node, Bound>), &getCounts<Space, Node, Bound>, GetCountsAct<Space, Node, Bound> >::type {};

template <typename Space, typename Node, typename Bound>
void setStopSearchFlag() {
  Registry<Space, Node, Bound>::gReg->setStopSearchFlag();
}
template <typename Space, typename Node, typename Bound>
struct SetStopFlagAct : hpx::actions::make_action<
  decltype(&setStopSearchFlag<Space, Node, Bound>), &setStopSearchFlag<Space, Node, Bound>, SetStopFlagAct<Space, Node, Bound> >::type {};

}

#endif
