// Contains the MetricStore class used to cache runtime meta data throughout a search
#pragma once

#include <atomic>
#include <algorithm>
#include <ctime>
#include <memory>
#include <limits>
#include <forward_list>
#include <vector>
#include <mutex>
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
  using TimesVecPtr = std::unique_ptr<std::vector<std::list<std::uint64_t> > >;
  using TimesVec = std::vector<std::list<std::uint64_t> >;

  // Regularity Metrics
  TimesVecPtr taskTimes;

  // Random number generator to determine if we collect a time (or not)
  boost::random::mt19937 gen;
  boost::random::uniform_int_distribution<> dist{1, 100};

  bool stopTakingTimes{false};

  // For node throughput
  MetricsVecPtr nodesVisited;

  // For Backtracking budget
  MetricsVecPtr backtracks;
	
	std::mutex m;

  // Counting pruning
  MetricsVecPtr prunes;
	
	static const unsigned DEF_SIZE = 50;
	static const unsigned TIME_DEPTHS = 8;

  MetricStore() = default;

  // Initialises the store for an analysis of runtime regulairty (and pruning for BnB)
  void init() {
    taskTimes = std::make_unique<TimesVec>(TIME_DEPTHS);
    prunes = std::make_unique<MetricsVecAtomic>(DEF_SIZE);
    backtracks = std::make_unique<MetricsVecAtomic>(DEF_SIZE);
		nodesVisited = std::make_unique<MetricsVecAtomic>(DEF_SIZE);
    gen.seed(std::time(NULL));
  }

  void updateTimes(const unsigned depth, const std::uint64_t time) {
   // if (stopTakingTimes) return;
		if (time >= 1) {
      // Generate random number and if below 75 then accept, else reject
//      if (dist(gen) <= 75) {
				auto size = 0UL;
				for (const auto & times : *taskTimes) {
					size += times.size();
				}
				// Only take 1000 samples
				//if (size >= 5000) {
 //         stopTakingTimes = true;
//          return;
   //     }
        const auto depthIdx = getDepthIndex(depth, TIME_DEPTHS);
        (*taskTimes)[depthIdx].push_front(time);
   	 	}
//		}
  }

  void updatePrunes(const unsigned depth, std::uint64_t p) {
    const auto depthIdx = getDepthIndex(depth, prunes->size());
    (*prunes)[depthIdx] += p;
  }

  void updateNodesVisited(const unsigned depth, std::uint64_t nodes) {
    const auto depthIdx = getDepthIndex(depth, nodesVisited->size());
    (*nodesVisited)[depthIdx] += nodes;
  }

  void updateBacktracks(const unsigned depth, std::uint64_t b) {
    const auto depthIdx = getDepthIndex(depth, backtracks->size());
    (*backtracks)[depthIdx] += b;
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

	void printTimes() {
    auto depth = 0;
		for (const auto & timeDepths : *taskTimes) {
      for (const auto & time : timeDepths) {
			  hpx::cout << "Depth :" << depth << " Time :" << time << hpx::endl;
      }
      depth++;
		}
	}

  ~MetricStore() = default;

private:

  inline unsigned getDepthIndex(const unsigned depth, const unsigned size) const {
    return (depth >= size) ? (size-1) : depth;
  }

  inline MetricsVec transformVec(const std::vector<std::atomic<std::uint64_t> > & vec) const {
    MetricsVec res;
    std::transform(vec.begin(), vec.end(), std::back_inserter(res),
    [](const auto & c) { return c.load(); });
    return res;
  }

};

MetricStore* MetricStore::store = new MetricStore;

void initMetricStore() {
  MetricStore::store->init();
}
struct InitMetricStoreAct : hpx::actions::make_direct_action<
  decltype(&initMetricStore), &initMetricStore, InitMetricStoreAct>::type {};

std::vector<std::uint64_t> getNodeCount() {
  return MetricStore::store->getNodeCount();
}
struct GetNodeCountAct : hpx::actions::make_direct_action<
  decltype(&getNodeCount), &getNodeCount, GetNodeCountAct>::type {};

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

