// Contains the MetricStore class used to cache runtime meta data throughout a search
#pragma once

#include <atomic>
#include <algorithm>
#include <memory>
#include <forward_list>
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
  using TimesVecPtr = std::unique_ptr<std::vector<std::list<std::uint64_t> > >;
  using TimesVec = std::vector<std::list<std::uint64_t> >;

  // Regularity Metrics
  TimesVecPtr taskTimes;

  // For node throughput
  MetricsVecPtr nodesVisited;

  // For Backtracking budget
  MetricsVecPtr backtracks;
	
  // Counting pruning
  MetricsVecPtr prunes;
	
	static const unsigned DEF_SIZE = 50;
	static const unsigned TIME_DEPTHS = 8;

  MetricStore() = default;

  void init() {
    taskTimes = std::make_unique<TimesVec>(TIME_DEPTHS);
    prunes = std::make_unique<MetricsVecAtomic>(DEF_SIZE);
    backtracks = std::make_unique<MetricsVecAtomic>(DEF_SIZE);
		nodesVisited = std::make_unique<MetricsVecAtomic>(DEF_SIZE);
  }

  void updateTimes(const unsigned depth, const std::uint64_t time) {
		if (time >= 1) {
        const auto depthIdx = getDepthIndex(depth, TIME_DEPTHS);
        (*taskTimes)[depthIdx].push_front(time);
   	 	}
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
    std::transform(vec.begin(), vec.begin()+newSize+1, std::back_inserter(res),
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


namespace hpx { namespace traits {

template <>
struct action_stacksize<YewPar::InitMetricStoreAct> {
  enum { value = threads::thread_stacksize_huge };
};

template <>
struct action_stacksize<YewPar::GetNodeCountAct> {
  enum { value = threads::thread_stacksize_huge };
};

template <>
struct action_stacksize<YewPar::GetBacktracksAct> {
  enum { value = threads::thread_stacksize_huge };
};

template <>
struct action_stacksize<YewPar::GetPrunesAct> {
  enum { value = threads::thread_stacksize_huge };
};

template <>
struct action_stacksize<YewPar::PrintTimesAct> {
  enum { value = threads::thread_stacksize_huge };
};

}}
