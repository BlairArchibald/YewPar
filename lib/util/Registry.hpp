#ifndef YEWPAR_REGISTRY_HPP
#define YEWPAR_REGISTRY_HPP

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iterator>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

#include <boost/random/random_device.hpp>

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
/*
  // Dissertation
  using MetricsVecPtr = std::unique_ptr<std::vector<std::atomic<std::uint64_t> > >;
  using MetricsVecAtomic = std::vector<std::atomic<std::uint64_t> >;
  using MetricsVec = std::vector<std::uint64_t>;
  using TimesVecPtr = std::unique_ptr<std::vector<std::vector<std::uint64_t> > >;
  using TimesVec = std::vector<std::vector<std::uint64_t> >;

  // Regularity Metrics
  MetricsVecPtr maxTimes;
  MetricsVecPtr minTimes;
  MetricsVecPtr runningAverages;
  MetricsVecPtr accumulatedTimes;
  TimesVecPtr workerTimes;

  // For node throughput
  MetricsVecPtr nodesVisited;
  
  // For Backtracking budget
  MetricsVecPtr backtracks;

  // Counting pruning
  MetricsVecPtr prunes;
*/
  // We construct this object globally at compile time (see below) so this can't
  // happen in the constructor and should instead be called as an action on each
  // locality.
  void initialise(Space space, Node root, Skeletons::API::Params<Bound> params, const unsigned includeMetrics=true) {
    this->space = space;
    this->root = root;
    this->params = params;
    this->localBound = params.initialBound;
    counts = std::make_unique<std::vector<std::atomic<std::uint64_t> > >(params.maxDepth + 1);
    /*
    if (includeMetrics) {
      maxTimes = std::make_unique<MetricsVecAtomic>(params.maxDepth + 1);
      minTimes = std::make_unique<MetricsVecAtomic>(params.maxDepth + 1);
      runningAverages = std::make_unique<MetricsVecAtomic>(params.maxDepth + 1);
      accumulatedTimes = std::make_unique<MetricsVecAtomic>(params.maxDepth + 1);
      workerTimes = std::make_unique<TimesVec>(params.maxDepth + 1);
      nodesVisited = std::make_unique<MetricsVecAtomic>(params.maxDepth + 1);
      backtracks = std::make_unique<MetricsVecAtomic>(params.maxDepth + 1);
      prunes = std::make_unique<MetricsVecAtomic>(params.maxDepth + 1);
    }*/

  }

  // Counting
  void updateCounts(std::vector<std::uint64_t> & cntMap) {
    for (auto i = 0; i <= params.maxDepth ; ++i) {
      // Addition happens atomically
      (*counts)[i] += cntMap[i];
    }
  }
/*
  void updateTimes(const unsigned depth, const std::uint64_t time) {
    (*accumulatedTimes)[depth] += time;
    if (time > 0) {
      (*workerTimes)[depth].push_back(time);
    }
    (*maxTimes)[depth] = (time > (*maxTimes)[depth].load()) ? time : (*maxTimes)[depth].load();
    (*minTimes)[depth] = (time < (*minTimes)[depth].load()) ? time : (*minTimes)[depth].load();
    (*runningAverages)[depth] = ((*runningAverages)[depth].load() + time)/(*nodesVisited)[depth];
  }

  void updatePrunes(const unsigned depth, std::uint64_t p) {
    (*prunes)[depth] += p;
  }

  void updateNodesVisited(const unsigned depth, std::uint64_t nodes) {
    (*nodesVisited)[depth] += nodes;
  }

  void updateBacktracks(const unsigned depth, std::uint64_t b) {
    (*backtracks)[depth] += b;
  }
*/
  std::vector<std::uint64_t> getCounts() {
    std::vector<std::uint64_t> res;
    std::transform(counts->begin(), counts->end(), [](const auto & c) { return c.load(); });
    return res;
  }
  /*
  MetricsVec getNodeCount() const {
    return transformVec(*nodesVisited);
  }

  MetricsVec getBacktracks() const {
    return transformVec(*backtracks);
  }

  MetricsVec getPrunes() const {
    return transformVec(*prunes);
  }

  MetricsVec getAccumulatedTimes() const {
    return transformVec(*accumulatedTimes);
  }

  MetricsVec getMinTimes() const {
    return transformVec(*minTimes);
  }

  MetricsVec getMaxTimes() const {
    return transformVec(*maxTimes);
  }

  MetricsVec getRunningAverages() const {
    return transformVec(*runningAverages);
  }

  TimesVec getTimes() const {
    return *workerTimes;
  }*/

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
/*template <typename Space, typename Node, typename Bound>
std::vector<std::uint64_t> getNodeCount() {
  return Registry<Space, Node, Bound>::gReg->getNodeCount();
}
template <typename Space, typename Node, typename Bound>
struct GetNodeCountAct : hpx::actions::make_direct_action<
  decltype(&getNodeCount<Space, Node, Bound>), &getNodeCount<Space, Node, Bound>, GetNodeCountAct<Space, Node, Bound> >::type {};

template <typename Space, typename Node, typename Bound>
std::vector<std::vector<std::uint64_t> > getTimes() {
  return Registry<Space, Node, Bound>::gReg->getTimes();
}
template <typename Space, typename Node, typename Bound>
struct GetTimesAct : hpx::actions::make_direct_action<
  decltype(&getTimes<Space, Node, Bound>), &getTimes<Space, Node, Bound>, GetTimesAct<Space, Node, Bound> >::type {};

template<typename Space, typename Node, typename Bound>
std::vector<std::uint64_t> getMaxTimes() {
  return Registry<Space, Node, Bound>::gReg->getMaxTimes();
}
template <typename Space, typename Node, typename Bound>
struct GetMaxTimesAct : hpx::actions::make_direct_action<
  decltype(&getMaxTimes<Space, Node, Bound>), &getMaxTimes<Space, Node, Bound>, GetMaxTimesAct<Space, Node, Bound> >::type {};

template<typename Space, typename Node, typename Bound>
std::vector<std::uint64_t> getMinTimes() {
  return Registry<Space, Node, Bound>::gReg->getMinTimes();
}
template <typename Space, typename Node, typename Bound>
struct GetMinTimesAct : hpx::actions::make_direct_action<
  decltype(&getMinTimes<Space, Node, Bound>), &getMinTimes<Space, Node, Bound>, GetMaxTimesAct<Space, Node, Bound> >::type {};

template <typename Space, typename Node, typename Bound>
std::vector<std::uint64_t> getRunningAverages() {
  return Registry<Space, Node, Bound>::gReg->getRunningAverages();
}
template <typename Space, typename Node, typename Bound>
struct GetRunningAveragesAct : hpx::actions::make_direct_action<
  decltype(&getRunningAverages<Space, Node, Bound>), &getRunningAverages<Space, Node, Bound>, GetRunningAveragesAct<Space, Node, Bound> >::type {};

template <typename Space, typename Node, typename Bound>
std::vector<std::uint64_t> getBacktracks() {
  return Registry<Space, Node, Bound>::gReg->getBacktracks();
}
template <typename Space, typename Node, typename Bound>
struct GetBacktracksAct : hpx::actions::make_direct_action<
  decltype(&getBacktracks<Space, Node, Bound>), &getBacktracks<Space, Node, Bound>, GetBacktracksAct<Space, Node, Bound> >::type {};

template <typename Space, typename Node, typename Bound>
std::vector<std::uint64_t> getPrunes() {
  return Registry<Space, Node, Bound>::gReg->getPrunes();
}
template <typename Space, typename Node, typename Bound>
struct GetPrunesAct : hpx::actions::make_direct_action<
  decltype(&getPrunes<Space, Node, Bound>), &getPrunes<Space, Node, Bound>, GetPrunesAct<Space, Node, Bound> >::type {};
*////////////////////////////////////////////////////////
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
