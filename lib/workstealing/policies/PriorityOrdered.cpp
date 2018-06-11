#include "PriorityOrdered.hpp"

#include <hpx/performance_counters/manage_counter_type.hpp>

namespace Workstealing { namespace Policies { namespace PriorityOrderedPerf {

std::atomic<std::uint64_t> perf_spawns(0);
std::atomic<std::uint64_t> perf_steals(0);
std::atomic<std::uint64_t> perf_failedSteals(0);

std::uint64_t get_and_reset(std::atomic<std::uint64_t> & cntr, bool reset) {
  auto res = cntr.load();
  if (reset) { cntr = 0; }
  return res;
}

std::uint64_t getSpawns (bool reset) { return get_and_reset(perf_spawns, reset);}
std::uint64_t getSteals(bool reset) { return get_and_reset(perf_steals, reset);}
std::uint64_t getFailedSteals(bool reset) { return get_and_reset(perf_failedSteals, reset);}

void registerPerformanceCounters() {
  hpx::performance_counters::install_counter_type(
      "/workstealing/PriorityOrdered/spawns",
      &getSpawns,
      "Number of tasks spawned on the global workpool"
                                                  );

  hpx::performance_counters::install_counter_type(
      "/workstealing/PriorityOrdered/steals",
      &getSteals,
      "Returns the number of tasks stolen from the global task queue"
                                                  );

  hpx::performance_counters::install_counter_type(
      "/workstealing/PriorityOrdered/failedSteals",
      &getFailedSteals,
      "Returns the number of failed steals from the global task queue"
                                                  );

}

}}}
