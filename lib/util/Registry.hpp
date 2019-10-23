#ifndef YEWPAR_REGISTRY_HPP
#define YEWPAR_REGISTRY_HPP

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <forward_list>
#include <iterator>
#include <memory>
#include <vector>
#include <mutex>

#include <hpx/lcos/local/composable_guard.hpp>
#include <hpx/runtime/actions/basic_action.hpp>
#include <hpx/traits/action_stacksize.hpp>
#include <hpx/lcos/local/channel.hpp>

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

  // Dissertation
  std::unique_ptr<std::atomic<std::uint64_t> > nodesVisited;
  std::unique_ptr<std::atomic<std::uint64_t> > prunes;
  std::unique_ptr<std::vector<std::vector<double> > > times;

  // We construct this object globally at compile time (see below) so this can't
  // happen in the constructor and should instead be called as an action on each
  // locality.
  void initialise(Space space, Node root, Skeletons::API::Params<Bound> params) {
    this->space = space;
    this->root = root;
    this->params = params;
    this->localBound = params.initialBound;
    counts = std::make_unique<std::vector<std::atomic<std::uint64_t> > >(params.maxDepth + 1);
    times = std::make_unique<std::vector<std::vector<double> > >(params.maxDepth + 1);
    nodesVisited = std::make_unique<std::atomic<std::uint64_t> >(0);
    prunes = std::make_unique<std::atomic<std::uint64_t> >(0);
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

  void addTime(const unsigned & depth, double & time)
  {
    (*times)[depth].push_back(time);
  }

  void updateCountOrPrune(std::uint64_t & count, const bool && incrVisit=true)
  {
    if (incrVisit) *nodesVisited += count;
    else *prunes += count;
  }

  std::uint64_t getCount() const
  {
    return nodesVisited->load();
  }

  std::vector<std::vector<double > > getTimes() const
  {
    return *times;
  }

  std::uint64_t getPrunes() const
  {
    return prunes->load();
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

///////////////////////////////////////////////////////
template <typename Space, typename Node, typename Bound>
std::uint64_t getCount() {
  return Registry<Space, Node, Bound>::gReg->getCount();
}
template <typename Space, typename Node, typename Bound>
struct GetCountAct : hpx::actions::make_direct_action<
  decltype(&getCount<Space, Node, Bound>), &getCount<Space, Node, Bound>, GetCountAct<Space, Node, Bound> >::type {};

template <typename Space, typename Node, typename Bound>
std::vector<std::vector<double> > getTimes() {
  return Registry<Space, Node, Bound>::gReg->getTimes();
}
template <typename Space, typename Node, typename Bound>
struct GetTimesAct : hpx::actions::make_direct_action<
  decltype(&getTimes<Space, Node, Bound>), &getTimes<Space, Node, Bound>, GetTimesAct<Space, Node, Bound> >::type {};

template <typename Space, typename Node, typename Bound>
std::uint64_t getPrunes() {
  return Registry<Space, Node, Bound>::gReg->getPrunes();
}
template <typename Space, typename Node, typename Bound>
void addTime(const unsigned depth, double time) {
  Registry<Space, Node, Bound>::gReg->addTime(depth, time);
}
template <typename Space, typename Node, typename Bound>
struct AddTimeAct : hpx::actions::make_direct_action<
  decltype(&addTime<Space, Node, Bound>), &addTime<Space, Node, Bound>, AddTimeAct<Space, Node, Bound> >::type {};
template <typename Space, typename Node, typename Bound>
struct GetPrunesAct : hpx::actions::make_direct_action<
  decltype(&getPrunes<Space, Node, Bound>), &getPrunes<Space, Node, Bound>, GetPrunesAct<Space, Node, Bound> >::type {};
///////////////////////////////////////////////////////
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

template <typename Space, typename Node, typename Bound>
void collectTimes()
{

}

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
