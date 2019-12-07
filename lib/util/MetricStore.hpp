// Contains the MetricStore class used to cache runtime meta data throughout a search
#pragma once

#include <atomic>
#include <algorithm>
#include <memory>
#include <limits>
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

  // For node throughput
  MetricsVecPtr nodesVisited;

  // For Backtracking budget
  MetricsVecPtr backtracks;

  // Counting pruning
  MetricsVecPtr prunes;

  MetricStore() = default;

  // Initialises the store
  void init(const unsigned maxDepth) {
    maxTimes = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
    minTimes = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
		for (int i = 0; i <= maxDepth; i++) {
			(*minTimes)[i] = ULLONG_MAX;
		}
    runningAverages = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
    accumulatedTimes = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
    nodesVisited = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
    backtracks = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
    prunes = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
  }

  void updateTimes(const unsigned depth, const std::uint64_t time) {
    (*accumulatedTimes)[depth] += time;
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

  ~MetricStore() = default;

private:

  inline std::vector<std::uint64_t> transformVec(
    const std::vector<std::atomic<std::uint64_t> > & vec
  ) const {
    MetricsVec res;
    std::transform(vec.begin(), vec.end(), std::back_inserter(res),
    [](const auto & c) { return c.load(); });
    return res;
  }

};
   
MetricStore* MetricStore::store = new MetricStore;

void initMetricStore(const unsigned maxDepth) {
  MetricStore::store->init(maxDepth);
}
struct InitMetricStoreAct : hpx::actions::make_direct_action<
  decltype(&initMetricStore), &initMetricStore, InitMetricStoreAct>::type {};

std::vector<std::uint64_t> getNodeCount() {
  return MetricStore::store->getNodeCount();
}
struct GetNodeCountAct : hpx::actions::make_direct_action<
  decltype(&getNodeCount), &getNodeCount, GetNodeCountAct>::type {};

std::vector<std::uint64_t> getMaxTimes() {
  return MetricStore::store->getMaxTimes();
}
struct GetMaxTimesAct : hpx::actions::make_direct_action<
  decltype(&getMaxTimes), &getMaxTimes, GetMaxTimesAct>::type {};

std::vector<std::uint64_t> getMinTimes() {
  return MetricStore::store->getMinTimes();
}
struct GetMinTimesAct : hpx::actions::make_direct_action<
  decltype(&getMinTimes), &getMinTimes, GetMinTimesAct>::type {};

std::vector<std::uint64_t> getRunningAverages() {
  return MetricStore::store->getRunningAverages();
}
struct GetRunningAveragesAct : hpx::actions::make_direct_action<
  decltype(&getRunningAverages), &getRunningAverages, GetRunningAveragesAct>::type {};

std::vector<std::uint64_t> getAccumulatedTimes() {
	return MetricStore::store->getAccumulatedTimes();
}
struct GetAccumulatedTimesAct : hpx::actions::make_direct_action<
	decltype(&getAccumulatedTimes), &getAccumulatedTimes, GetAccumulatedTimesAct>::type {};

std::vector<std::uint64_t> getBacktracks() {
  return MetricStore::store->getBacktracks();
}
struct GetBacktracksAct : hpx::actions::make_direct_action<
  decltype(&getBacktracks), &getBacktracks, GetBacktracksAct>::type {};

std::vector<std::uint64_t> getPrunes() {
  return MetricStore::store->getPrunes();
}
struct GetPrunesAct : hpx::actions::make_direct_action<
  decltype(&getPrunes), &getPrunes, GetPrunesAct>::type {};
}
