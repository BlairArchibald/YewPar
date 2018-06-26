#include "SearchManager.hpp"
#include "hpx/runtime/components/component_factory_base.hpp"  // for HPX_REG...
#include "hpx/runtime/components/static_factory_data.hpp"     // for static_...

HPX_REGISTER_COMPONENT_MODULE();

namespace Workstealing { namespace Policies { namespace SearchManagerPerf {

std::uint64_t get_and_reset(std::atomic<std::uint64_t> & cntr, bool reset) {
  auto res = cntr.load();
  if (reset) { cntr = 0; }
  return res;
}

std::uint64_t getLocalSteals(bool reset) { return get_and_reset(perf_localSteals, reset);}
std::uint64_t getDistributedSteals (bool reset) { return get_and_reset(perf_distributedSteals, reset);}
std::uint64_t getFailedLocalSteals(bool reset) { return get_and_reset(perf_failedLocalSteals, reset);}
std::uint64_t getFailedDistributedSteals(bool reset) { return get_and_reset(perf_failedDistributedSteals, reset);}

void registerPerformanceCounters() {
  hpx::performance_counters::install_counter_type(
      "/workstealing/SearchManager/localSteals",
      &getLocalSteals,
      "Returns the number of tasks stolen from another thread on the same locality"
                                                  );

  hpx::performance_counters::install_counter_type(
      "/workstealing/SearchManager/distributedSteals",
      &getDistributedSteals,
      "Returns the number of tasks stolen from another thread on another locality"
                                                  );

  hpx::performance_counters::install_counter_type(
      "/workstealing/SearchManager/localFailedSteals",
      &getFailedLocalSteals,
      "Returns the number of failed steals from this locality "
                                                  );

  hpx::performance_counters::install_counter_type(
      "/workstealing/SearchManager/distributedFailedSteals",
      &getFailedDistributedSteals,
      "Returns the number of failed steals from another locality "
                                                  );
}

// Debugging information that doesn't fit a counter format
void printDistributedStealsList() {
  for (const auto &p : distributedStealsList) {
    std::cout
        << (boost::format("Steal %1% - %2% , success: %3%")
            % static_cast<std::int64_t>(hpx::get_locality_id())
            % static_cast<std::int64_t>(hpx::naming::get_locality_id_from_id(p.first))
            % p.second)
        << std::endl;
  }
}

void printChunkSizeList() {
  for (const auto &c : chunkSizeList) {
    std::cout
        << (boost::format("%1% Stole Chunk of Size %2%")
            % static_cast<std::int64_t>(hpx::get_locality_id())
            % c)
        << std::endl;
  }
}

}}}
