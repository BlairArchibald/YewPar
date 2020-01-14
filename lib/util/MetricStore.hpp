// Contains the MetricStore class used to cache runtime meta data throughout a search
#pragma once

#include <atomic>
#include <algorithm>
#include <ctime>
#include <memory>
#include <limits>
#include <vector>

#include <boost/random.hpp>

#include <hpx/runtime/actions/basic_action.hpp>
#include <hpx/traits/action_stacksize.hpp>

#include "skeletons/API.hpp"

namespace YewPar {

struct MetricStore {
  static MetricStore* store;

  using MetricsVecPtr = std::unique_ptr<std::vector<std::atomic<std::uint64_t> > >;
  using MetricsVecAtomic = std::vector<std::atomic<std::uint64_t> >;
  using MetricsVec = std::vector<std::uint64_t>;
  using TimesVecPtr = std::unique_ptr<std::vector<std::uint64_t> >;
  using TimesVec = std::vector<std::uint64_t>;

  // Regularity Metrics
  TimesVecPtr taskTimes;
  // Random number generator to determine if we collect a time (or not)
  boost::random::mt19937 gen;
  boost::random::bernoulli_distribution<> dist;
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

  // Initialises the store for an analysis of runtime regulairty (and pruning for BnB)
  void init(const unsigned maxDepth, const unsigned scaling, const unsigned regularity) {
    if (regularity) {
      taskTimes = std::make_unique<TimesVec>();
      prunes = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
      backtracks = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
			std::time_t now = std::time(NULL);
    	gen.seed(now);
    }
    if (scaling) {
      nodesVisited = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
    }
  }

  void updateTimes(const unsigned depth, const std::uint64_t time) {
		if (time >= 1 && taskTimes->size() < 100) {
      // Generate random number and if below 0.7 then accept, else reject
      if (dist(gen) < 0.7) {
        taskTimes->push_back(time);
        maxDepthBuckets = depth > maxDepthBuckets.load() ? depth : maxDepthBuckets.load();
      }
		}
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

  TimesVec getTimeBuckets() {
    taskTimes->resize(maxDepthBuckets + 1);
    return *taskTimes;
  }

	void printTimes() {
		taskTimes->resize(maxDepthBuckets + 1);
		for (const auto & time : *taskTimes) {
			hpx::cout << "Time :" << time << hpx::endl;
		}
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

void initMetricStore(const unsigned maxDepth, const unsigned scaling, const unsigned regularity) {
  MetricStore::store->init(maxDepth, scaling, regularity);
}
struct InitMetricStoreAct : hpx::actions::make_direct_action<
  decltype(&initMetricStore), &initMetricStore, InitMetricStoreAct>::type {};

std::vector<std::uint64_t> getNodeCount() {
  return MetricStore::store->getNodeCount();
}
struct GetNodeCountAct : hpx::actions::make_direct_action<
  decltype(&getNodeCount), &getNodeCount, GetNodeCountAct>::type {};

std::vector<std::uint64_t> getTimeBuckets() {
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

void printTimes() {
	MetricStore::store->printTimes();
}
struct PrintTimesAct : hpx::actions::make_direct_action<
	decltype(&printTimes), &printTimes, PrintTimesAct>::type {};

}

