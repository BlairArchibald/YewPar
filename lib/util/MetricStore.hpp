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
  MetricsVecPtr accumulatedTimes;
  TimesVecPtr timeBuckets;

  // For node throughput
  MetricsVecPtr nodesVisited;

  // For Backtracking budget
  MetricsVecPtr backtracks;

  // Counting pruning
  MetricsVecPtr prunes;

  MetricStore() = default;

  // Initialises the store
  void init(const unsigned maxDepth) {
    accumulatedTimes = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
    timeBuckets = std::make_unique<TimesVec>(maxDepth + 1, std::vector<std::uint64_t>(13));
    nodesVisited = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
    backtracks = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
    prunes = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
  }

  void updateTimes(const unsigned depth, const std::uint64_t time) {
    (*accumulatedTimes)[depth] += time;
    if (time < 1) {
      (*timeBuckets)[depth][0] += 1;
    } else if (time >= 1 && time < 50) {
      (*timeBuckets)[depth][1] += 1;
    } else if (time >= 50 && time < 100) {
      (*timeBuckets)[depth][2] += 1;
    } else if (time >= 100 && time < 250) {
      (*timeBuckets)[depth][3] += 1;
    } else if (time >= 250 && time < 500) {
      (*timeBuckets)[depth][4] += 1;
    } else if (time >= 500 && time < 1000) {
      (*timeBuckets)[depth][5] += 1;
    } else if (time >= 1000 && time < 2000) {
      (*timeBuckets)[depth][6] += 1;
    } else if (time >= 2000 && time < 4000) {
      (*timeBuckets)[depth][7] += 1;
    } else if (time >= 4000 && time < 8000) {
      (*timeBuckets)[depth][8] += 1;
    } else if (time >= 8000 && time < 16000) {
      (*timeBuckets)[depth][9] += 1;
    } else if (time >= 16000 && time < 32000) {
      (*timeBuckets)[depth][10] += 1;
    } else if (time >= 32000 && time < 64000) {
      (*timeBuckets)[depth][11] += 1;
    } else {
      (*timeBuckets)[depth][12] += 1;
    }
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

  TimesVec getTimeBuckets() const {
    return *timeBuckets;
  }

  ~MetricStore() = default;

private:

  inline MetricsVec transformVec(
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

std::vector<std::uint64_t> getAccumulatedTimes() {
	return MetricStore::store->getAccumulatedTimes();
}
struct GetAccumulatedTimesAct : hpx::actions::make_direct_action<
	decltype(&getAccumulatedTimes), &getAccumulatedTimes, GetAccumulatedTimesAct>::type {};

std::vector<std::vector<std::uint64_t> > getTimeBuckets() {
  return MetricStore::store->getTimeBuckets();
}
struct GetTimeBucketsAct : hpx::actions::make_direct_action<
  decltype(&getTimeBuckets), &getTimeBuckets, GetTimeBucketsAct>::type {};

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
