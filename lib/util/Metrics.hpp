#pragma once

#include <memory>
#include <vector>
#include <set>

namespace YewPar {


// Dissertation
  using MetricsVecPtr = std::unique_ptr<std::vector<std::atomic<std::uint64_t> > >;
  using MetricsVec = std::vector<std::uint64_t>;
  using MetricsSetPtr = std::unique_ptr<std::vector<std::set<std::uint64_t> > >;
  using MetricsSet = std::vector<std::set<std::uint64_t> >;

  // Regularity Metrics
  MetricsVecPtr maxTimes;
  MetricsVecPtr minTimes;
  MetricsVecPtr runningAverages;
  MetricsVecPtr accumulatedTimes;
  MetricsSetPtr times;