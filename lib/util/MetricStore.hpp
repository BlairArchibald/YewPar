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
  TimesVecPtr timeBuckets;
  // Keep track of the max depth reached for all metric vectors so we can resize later to avoid excessive amounts of print statements
  std::atomic<unsigned> maxDepthBuckets;

  // For node throughput
  MetricsVecPtr nodesVisited;
  std::atomic<unsigned> maxDepthNodesVisited;

  // For Backtracking budget
  MetricsVecPtr backtracks;
  std::atomic<unsigned> maxDepthBacktracks;

  // Counting pruning
  MetricsVecPtr prunes;
  std::atomic<unsigned> maxDepthPrunes;

  MetricStore() = default;

  // Initialises the store
  void init(const unsigned maxDepth) {
    timeBuckets = std::make_unique<TimesVec>(maxDepth + 1, std::vector<std::uint64_t>(13));
    nodesVisited = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
    backtracks = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
    prunes = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
  }

  void updateTimes(const unsigned depth, const std::uint64_t time) {
    if (time < 1) {
      (*timeBuckets)[depth][0]++;
    } else if (time >= 1 && time < 50) {
      (*timeBuckets)[depth][1]++;
    } else if (time >= 50 && time < 100) {
      (*timeBuckets)[depth][2]++;
    } else if (time >= 100 && time < 250) {
      (*timeBuckets)[depth][3]++;
    } else if (time >= 250 && time < 500) {
      (*timeBuckets)[depth][4]++;
    } else if (time >= 500 && time < 1000) {
      (*timeBuckets)[depth][5]++;
    } else if (time >= 1000 && time < 2000) {
      (*timeBuckets)[depth][6]++;
    } else if (time >= 2000 && time < 4000) {
      (*timeBuckets)[depth][7]++;
    } else if (time >= 4000 && time < 8000) {
      (*timeBuckets)[depth][8]++;
    } else if (time >= 8000 && time < 16000) {
      (*timeBuckets)[depth][9]++;
    } else if (time >= 16000 && time < 32000) {
      (*timeBuckets)[depth][10]++;
    } else if (time >= 32000 && time < 64000) {
      (*timeBuckets)[depth][11]++;
    } else {
      (*timeBuckets)[depth][12]++;
    }

    maxDepthBuckets = depth > maxDepthBuckets.load() ? depth : maxDepthBuckets.load();
  }

  void updatePrunes(const unsigned depth, std::uint64_t p) {
    (*prunes)[depth] += p;
    maxDepthPrunes = depth > maxDepthPrunes.load() ? depth : maxDepthPrunes.load();
  }

  void updateNodesVisited(const unsigned depth, std::uint64_t nodes) {
    (*nodesVisited)[depth] += nodes;
    maxDepthNodesVisited = depth > maxDepthNodesVisited.load() ? depth : maxDepthNodesVisited.load();
  }

  void updateBacktracks(const unsigned depth, std::uint64_t b) {
    (*backtracks)[depth] += b;
    maxDepthBacktracks = depth > maxDepthBacktracks.load() ? depth : maxDepthBacktracks.load();
  }

  MetricsVec getNodeCount() const {
    return transformVec(*nodesVisited, maxDepthNodesVisited);
  }

  MetricsVec getBacktracks() const {
    return transformVec(*backtracks, maxDepthBacktracks);
	}

  MetricsVec getPrunes() const {
    return transformVec(*prunes, maxDepthPrunes);
  }

  TimesVec getTimeBuckets() const {
    timeBuckets->resize(maxDepthBuckets + 1);
    return *timeBuckets;
  }

  ~MetricStore() = default;

private:


  inline MetricsVec transformVec(
    const std::vector<std::atomic<std::uint64_t> > & vec,
    const unsigned newSize
  ) const {
    MetricsVec res;
    std::transform(vec.begin(), vec.end(), std::back_inserter(res),
    [](const auto & c) { return c.load(); });
    res.resize(newSize + 1);
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
