// Contains the MetricStore class used to cache runtime meta data throughout a search
#pragma once

#include <atomic>
#include <algorithm>
#include <ctime>
#include <memory>
#include <limits>
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
  using TimesVecPtr = std::unique_ptr<std::vector<std::unique_ptr<std::vector<std::uint64_t> > > >;
  using TimesVec = std::vector<std::unique_ptr<std::vector<std::uint64_t> > >;

  // Regularity Metrics
  TimesVecPtr taskTimes;

  // Random number generator to determine if we collect a time (or not)
  boost::random::mt19937 gen;
  boost::random::uniform_int_distribution<> dist{1, 100};

  // For node throughput
  MetricsVecPtr nodesVisited;
  std::atomic<unsigned> maxDepthNodesVisited;

  // For Backtracking budget
  MetricsVecPtr backtracks;
  std::atomic<unsigned> maxDepthBacktracks;
	
	std::mutex m;

  // Counting pruning
  MetricsVecPtr prunes;
  std::atomic<unsigned> maxDepthPrunes;
	
	static constexpr unsigned DEF_SIZE	= 100;
	static constexpr unsigned TIME_DEPTHS = 8;

  MetricStore() = default;

  // Initialises the store for an analysis of runtime regulairty (and pruning for BnB)
  void init(const unsigned maxDepth, const unsigned scaling, const unsigned metrics) {
    if (metrics) {
      taskTimes = std::make_unique<TimesVec>(TIME_DEPTHS);
      for (unsigned i = 0; i < TIME_DEPTHS; i++) {
				(*taskTimes)[i] = std::make_unique<MetricsVec>();
			}
      prunes = std::make_unique<MetricsVecAtomic>(DEF_SIZE);
      backtracks = std::make_unique<MetricsVecAtomic>(DEF_SIZE);
    	gen.seed(std::time(NULL));
    }
    if (scaling) {
      nodesVisited = std::make_unique<MetricsVecAtomic>(maxDepth + 1);
    }
  }

  void updateTimes(const unsigned depth, const std::uint64_t time) {
		std::unique_lock<std::mutex> lock(m);
		if (time >= 1) {
      // Generate random number and if below 75 then accept, else reject
      if (dist(gen) <= 75) {
				unsigned size = 0;
				for (const auto & times : *taskTimes) {
					size += times->size();
				}
				// Only take 1000 samples
				if (size >= 1000) { return; }
        const auto depthIdx = (depth >= TIME_DEPTHS) ? (TIME_DEPTHS-1) : (depth > 0) ? (depth-1) : 0;
        (*taskTimes)[depthIdx]->push_back(time);
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

	void printTimes() {
    auto depth = 0;
		for (const auto & timeDepths : *taskTimes) {
      for (const auto & time : *timeDepths) {
			  hpx::cout << "Depth :" << depth << " Time :" << time << hpx::endl;
      }
      depth++;
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

void initMetricStore(const unsigned maxDepth, const unsigned scaling, const unsigned metrics) {
  MetricStore::store->init(maxDepth, scaling, metrics);
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

