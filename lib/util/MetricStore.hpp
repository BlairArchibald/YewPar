// Contains the MetricStore class used to cache runtime meta data throughout a search
#pragma once

#include <atomic>
#include <algorithm>
#include <memory>
#include <vector>

#include <hpx/runtime/actions/basic_action.hpp>
#include <hpx/traits/action_stacksize.hpp>

#include "skeletons/API.hpp"

namespace YewPar {

  struct MetricStore {
    static MetricStore* store;

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

    using MetricsStore() = default;

    // Initialises the store
    void init(const unsigned maxDepth) {
      maxTimes = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
      minTimes = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
      runningAverages = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
      accumulatedTimes = std::make_unique<MetricsVecAtomic>(params.maxDepth + 1);
      workerTimes = std::make_unique<TimesVec>(params.maxDepth + 1);
      nodesVisited = std::make_unique<MetricsVecAtomic>(params.maxDepth + 1);
      backtracks = std::make_unique<MetricsVecAtomic>(params.maxDepth + 1);
      prunes = std::make_unique<MetricsVecAtomic>(params.maxDepth + 1);
    }

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
    }

    using ~MetricsStore() = default;

private:

  inline MetricsVec transformVec(
    const std::vector<std::atomic<std::uint64_t> > & vec
  ) const {
    MetricsVec res;
    std::transform(vec.begin(), vec.end(), std::back_inserter(res),
    [](const auto & c) { return c.load(); });
    return res;
  }

}

MetricStore::store = new MetricsStore;

void initMetricStore(const unsigned maxDepth) {
  MetricStore::store->init(maxDepth);
}
struct InitMetricStoreAct : hpx::actions::make_direct_action<
  decltype(&initMetricStore), &initMetricStore, initMetricStoreAct::type> >::type {};

std::vector<std::uint64_t> getNodeCount() {
  return MetricStore::store->getNodeCount();
}
struct GetNodeCountAct : hpx::actions::make_direct_action<
  decltype(&getNodeCount<Space, Node, Bound>), &getNodeCount<Space, Node, Bound>, GetNodeCountAct<Space, Node, Bound> >::type {};

std::vector<std::vector<std::uint64_t> > getTimes() {
  return MetricStore::store->getTimes();
}
struct GetTimesAct : hpx::actions::make_direct_action<
  decltype(&getTimes<Space, Node, Bound>), &getTimes<Space, Node, Bound>, GetTimesAct<Space, Node, Bound> >::type {};

template<typename Space, typename Node, typename Bound>
std::vector<std::uint64_t> getMaxTimes() {
  return MetricStore::store->getMaxTimes();
}
struct GetMaxTimesAct : hpx::actions::make_direct_action<
  decltype(&getMaxTimes<Space, Node, Bound>), &getMaxTimes<Space, Node, Bound>, GetMaxTimesAct<Space, Node, Bound> >::type {};

template<typename Space, typename Node, typename Bound>
std::vector<std::uint64_t> getMinTimes() {
  return MetricStore::store->getMinTimes();
}
struct GetMinTimesAct : hpx::actions::make_direct_action<
  decltype(&getMinTimes<Space, Node, Bound>), &getMinTimes<Space, Node, Bound>, GetMaxTimesAct<Space, Node, Bound> >::type {};

std::vector<std::uint64_t> getRunningAverages() {
  return MetricStore::store->getRunningAverages();
}
struct GetRunningAveragesAct : hpx::actions::make_direct_action<
  decltype(&getRunningAverages<Space, Node, Bound>), &getRunningAverages<Space, Node, Bound>, GetRunningAveragesAct<Space, Node, Bound> >::type {};

std::vector<std::uint64_t> getBacktracks() {
  return MetricStore::store->getBacktracks();
}
struct GetBacktracksAct : hpx::actions::make_direct_action<
  decltype(&getBacktracks<Space, Node, Bound>), &getBacktracks<Space, Node, Bound>, GetBacktracksAct<Space, Node, Bound> >::type {};

std::vector<std::uint64_t> getPrunes() {
  return MetricStore::store->getPrunes();
}
struct GetPrunesAct : hpx::actions::make_direct_action<
  decltype(&getPrunes<Space, Node, Bound>), &getPrunes<Space, Node, Bound>, GetPrunesAct<Space, Node, Bound> >::type {};

namespace hpx { namespace traits {
struct action_stacksize<YewPar::InitMetricStoreAct> {
  enum { value = threads::thread_stacksize_huge };
};

}}
