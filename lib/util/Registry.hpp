#ifndef YEWPAR_REGISTRY_HPP
#define YEWPAR_REGISTRY_HPP

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <forward_list>
#include <iterator>
#include <memory>
#include <vector>

#include <hpx/runtime/actions/basic_action.hpp>
#include <hpx/traits/action_stacksize.hpp>

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
  hpx::naming::id_type foundPromiseId;

  // Counting Nodes
  using countMapT = std::vector<std::atomic<std::uint64_t> >;
  std::unique_ptr<std::vector<std::atomic<std::uint64_t> > > counts;

  // Counts for Dissertation
  std::unique_ptr<std::vector<std::vector<double> > > timeCounts;
  std::unique_ptr<std::atomic<std::uint64_t> > nodesVisited;

  std::atomic<unsigned> maxDepth;
  std::atomic_flag lock{ATOMIC_FLAG_INIT};
  std::atomic_flag lockCount{ATOMIC_FLAG_INIT};

  // We construct this object globally at compile time (see below) so this can't
  // happen in the constructor and should instead be called as an action on each
  // locality.
  void initialise(Space space, Node root, Skeletons::API::Params<Bound> params) {
    this->space = space;
    this->root = root;
    this->params = params;
    this->localBound = params.initialBound;
    maxDepth.store(params.maxDepth);
    counts = std::make_unique<std::vector<std::atomic<std::uint64_t> > >(params.maxDepth + 1);
    timeCounts = std::make_unique<std::vector<std::vector<double> > >(params.maxDepth + 1);
    nodesVisited = std::make_unique<std::atomic<std::uint64_t> >(0);
  }

  // Counting
  void updateCounts(std::vector<std::uint64_t> & cntMap) {
    for (auto i = 0; i <= params.maxDepth ; ++i) {
      // Addition happens atomically
      (*counts)[i] += cntMap[i];
    }
  }

  std::vector<std::uint64_t>  getCounts() {
    // Convert std::atomic<T> -> T by loading it
    std::vector<std::uint64_t> res;
    std::transform(counts->begin(), counts->end(), std::back_inserter(res),
    [](const auto & c) { return c.load(); });
    return res;
  }

  std::vector<std::vector<double> > *getTimes()
  {
    return timeCounts.get();
  }

  void addTime(int depth, double time)
  {
    // Use spin locks for efficiency
    while (lock.test_and_set(std::memory_order_acquire))
      ;
    (*timeCounts)[depth].push_back(time);
    lock.clear(std::memory_order_release);
  }

  void updateCount()
  {
    while (lockCount.test_and_set(std::memory_order_release))
      ;
    nodesVisited->store(nodesVisited->load() + 1.0);
    lockCount.clear(std::memory_order_release);
  }

  // BNB
  template <typename Cmp>
  void updateRegistryBound(Bound bnd) {
    while (true) {
      auto curBound = localBound.load();
      Cmp cmp;
      if (!cmp(bnd, curBound)) {
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
struct InitRegistryAct : hpx::actions::make_direct_action<
  decltype(&initialiseRegistry<Space, Node, Bound>), &initialiseRegistry<Space, Node, Bound>, InitRegistryAct<Space, Node, Bound> >::type {};

template <typename Space, typename Node, typename Bound>
std::vector<std::uint64_t> getCounts() {
  return Registry<Space, Node, Bound>::gReg->getCounts();
}
template <typename Space, typename Node, typename Bound>
struct GetCountsAct : hpx::actions::make_direct_action<
  decltype(&getCounts<Space, Node, Bound>), &getCounts<Space, Node, Bound>, GetCountsAct<Space, Node, Bound> >::type {};

template <typename Space, typename Node, typename Bound>
void setStopSearchFlag() {
  Registry<Space, Node, Bound>::gReg->setStopSearchFlag();
}
template <typename Space, typename Node, typename Bound>
struct SetStopFlagAct : hpx::actions::make_direct_action<
  decltype(&setStopSearchFlag<Space, Node, Bound>), &setStopSearchFlag<Space, Node, Bound>, SetStopFlagAct<Space, Node, Bound> >::type {};

template <typename Space, typename Node, typename Bound, typename Cmp>
void updateRegistryBound(Bound bnd) {
  auto reg = Registry<Space, Node, Bound>::gReg;
  (*reg).template updateRegistryBound<Cmp>(bnd);
}
template <typename Space, typename Node, typename Bound, typename Cmp>
struct UpdateRegistryBoundAct : hpx::actions::make_direct_action<
  decltype(&updateRegistryBound<Space, Node, Bound, Cmp>), &updateRegistryBound<Space, Node, Bound, Cmp>, UpdateRegistryBoundAct<Space, Node, Bound, Cmp> >::type {};

template <typename Space, typename Node, typename Bound>
void updateGlobalIncumbent(hpx::naming::id_type inc) {
  Registry<Space, Node, Bound>::gReg->globalIncumbent = inc;
}
template <typename Space, typename Node, typename Bound>
struct UpdateGlobalIncumbentAct : hpx::actions::make_direct_action<
  decltype(&updateGlobalIncumbent<Space, Node, Bound>), &updateGlobalIncumbent<Space, Node, Bound>, UpdateGlobalIncumbentAct<Space, Node, Bound> >::type {};

template <typename Space, typename Node, typename Bound>
void setFoundPromiseId(hpx::naming::id_type id) {
  Registry<Space, Node, Bound>::gReg->foundPromiseId = id;
}
template <typename Space, typename Node, typename Bound>
struct SetFoundPromiseIdAct : hpx::actions::make_direct_action<
  decltype(&setFoundPromiseId<Space, Node, Bound>), &setFoundPromiseId<Space, Node, Bound>, SetFoundPromiseIdAct<Space, Node, Bound> >::type {};

} // YewPar

namespace hpx { namespace traits {
template <typename Space, typename Node, typename Bound>
struct action_stacksize<YewPar::InitRegistryAct<Space, Node, Bound> > {
  enum { value = threads::thread_stacksize_huge };
};

template <typename Space, typename Node, typename Bound>
struct action_stacksize<YewPar::SetFoundPromiseIdAct<Space, Node, Bound> > {
  enum { value = threads::thread_stacksize_huge };
};

template <typename Space, typename Node, typename Bound>
struct action_stacksize<YewPar::UpdateGlobalIncumbentAct<Space, Node, Bound> > {
  enum { value = threads::thread_stacksize_huge };
};

template <typename Space, typename Node, typename Bound, typename Cmp>
struct action_stacksize<YewPar::UpdateRegistryBoundAct<Space, Node, Bound, Cmp> > {
  enum { value = threads::thread_stacksize_huge };
};

template <typename Space, typename Node, typename Bound>
struct action_stacksize<YewPar::SetStopFlagAct<Space, Node, Bound> > {
  enum { value = threads::thread_stacksize_huge };
};

template <typename Space, typename Node, typename Bound>
struct action_stacksize<YewPar::GetCountsAct<Space, Node, Bound> > {
  enum { value = threads::thread_stacksize_huge };
};

}}

#endif
