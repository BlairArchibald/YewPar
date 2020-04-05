// Contains the MetricStore class used to cache runtime meta data throughout a search
#pragma once

#include <array>
#include <atomic>
#include <algorithm>
#include <memory>
#include <numeric>
#include <forward_list>
#include <vector>

#include <hpx/runtime/actions/basic_action.hpp>
#include <hpx/traits/action_stacksize.hpp>

#include "skeletons/API.hpp"

namespace YewPar {

struct MetricStore {
  static MetricStore* store;

  using MetricsArrayPtr = std::unique_ptr<std::array<std::atomic<std::uint64_t>, DEF_SIZE> >;
  using MetricsArrayAtomic = std::array<std::atomic<std::uint64_t>, DEF_SIZE >;
  using MetricsArray = std::array<std::uint64_t, DEF_SIZE>;
  using TimesVecPtr = std::unique_ptr<std::vector<std::forward_list<std::uint64_t> > >;
  using TimesVec = std::vector<std::forward_list<std::uint64_t> >;

  // Regularity Metrics
  TimesVecPtr taskTimes;
	MetricsArrayPtr tasks;	

  // For node throughput
  MetricsArrayPtr nodesVisited;

  // For Backtracking budget
  MetricsArrayPtr backtracks;
	
  // Counting pruning
  MetricsArrayPtr prunes;
	
	static const unsigned DEF_SIZE = 100;

  MetricStore() = default;

  void init() {
    taskTimes = std::make_unique<TimesVec>(DEF_SIZE);
    backtracks = std::make_unique<MetricsArrayAtomic>(DEF_SIZE);
		nodesVisited = std::make_unique<MetricsArrayAtomic>(DEF_SIZE);
		tasks = std::make_unique<MetricsArrayAtomic>(DEF_SIZE);
  }

  /* For conveniance */
  void updateMetrics(const unsigned depth, const std::uint64_t t, 
                     const std::uint64_t p, const std::uint64_t n,
                     const std::uint64_t b) {
    updateTimes(depth, t);
    updatePrunes(depth, p);
    updateNodesVisited(depth, n);
    updateBacktracks(depth, b);
  }

  void updateTimes(const unsigned depth, std::uint64_t time) {
		if (time >= 1) {
			const auto depthIdx = depth < taskTimes->size() ? depth : taskTimes->size()-1;
      (*taskTimes)[depthIdx].push_front(time);
			(*tasks)[depthIdx]++;
   	}
  }

  void updatePrunes(const unsigned depth, std::uint64_t p) {
    updateMetric(*prunes, p, depth);
  }

  void updateNodesVisited(const unsigned depth, std::uint64_t n) {
    updateMetric(*nodesVisited, n, depth);
  }

  void updateBacktracks(const unsigned depth, std::uint64_t b) {
    updateMetric(*backtracks, b, depth);
  }

  MetricsArray getNodeCount() const {
    return transformVec(*nodesVisited);
  }

  MetricsArray getBacktracks() const {
    return transformVec(*backtracks);
	}

  MetricsArray getPrunes() const {
    return transformVec(*prunes);
  }

  MetricsArray getTotalTasks () const {
		return transformVec(*tasks);
  }

	void printTimes() {
    auto depth = 0;
		for (const auto & timeDepths : *taskTimes) {
      for (const auto & time : timeDepths) {
			  if (time >= 1) {
					hpx::cout << "Depth :" << depth << " Time :" << time << hpx::endl;
      	}
			}
      depth++;
		}
	}

  ~MetricStore() = default;

private:

  inline void updateMetric(MetricsArrayAtomic & ms, const std::uint64_t & m, const unsigned depth) {
		auto depthIdx = depth < ms.size() ? depth : ms.size()-1;
    ms[depthIdx] += m;
  }

  inline MetricsArray transformVec(const MetricsArrayAtomic & vec) const {
    MetricsArray res;
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

std::array<std::uint64_t> getNodeCount() {
  return MetricStore::store->getNodeCount();
}
struct GetNodeCountAct : hpx::actions::make_direct_action<
  decltype(&getNodeCount), &getNodeCount, GetNodeCountAct>::type {};

std::array<std::uint64_t> getBacktracks() {
  return MetricStore::store->getBacktracks();
}
struct GetBacktracksAct : hpx::actions::make_direct_action<
  decltype(&getBacktracks), &getBacktracks, GetBacktracksAct>::type {};

std::array<std::uint64_t> getPrunes() {
  return MetricStore::store->getPrunes();
}
struct GetPrunesAct : hpx::actions::make_direct_action<
  decltype(&getPrunes), &getPrunes, GetPrunesAct>::type {};

void printTimes() {
	MetricStore::store->printTimes();
}
struct PrintTimesAct : hpx::actions::make_direct_action<
	decltype(&printTimes), &printTimes, PrintTimesAct>::type {};

std::vector<std::uint64_t> getTotalTasks() {
  return MetricStore::store->getTotalTasks();
}
struct GetTotalTasksAct : hpx::actions::make_direct_action<
  decltype(&getTotalTasks), &getTotalTasks, GetTotalTasksAct>::type {};

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

template <>
struct action_stacksize<YewPar::GetTotalTasksAct> {
  enum { value = threads::thread_stacksize_huge };
};

}}
