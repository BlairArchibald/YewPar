#include "DepthPoolPolicy.hpp"

#include <hpx/util/function.hpp>
#include <hpx/runtime/find_here.hpp>
#include <hpx/performance_counters/manage_counter_type.hpp>

#include <memory>

#include "util/util.hpp"

namespace Workstealing { namespace Policies {

namespace DepthPoolPolicyPerf {

std::atomic<std::uint64_t> perf_localSteals(0);
std::atomic<std::uint64_t> perf_distributedSteals(0);
std::atomic<std::uint64_t> perf_failedLocalSteals(0);
std::atomic<std::uint64_t> perf_failedDistributedSteals(0);

std::uint64_t get_and_reset(std::atomic<std::uint64_t> & cntr, bool reset) {
  auto res = cntr.load();
  if (reset) { cntr = 0; }
  return res;
}

std::uint64_t getLocalSteals(bool reset) {get_and_reset(perf_localSteals, reset);}
std::uint64_t getDistributedSteals (bool reset) {get_and_reset(perf_distributedSteals, reset);}
std::uint64_t getFailedLocalSteals(bool reset) {get_and_reset(perf_failedLocalSteals, reset);}
std::uint64_t getFailedDistributedSteals(bool reset) {get_and_reset(perf_failedDistributedSteals, reset);}

void registerPerformanceCounters() {
  hpx::performance_counters::install_counter_type(
      "/workstealing/depthpool/localSteals",
      &getLocalSteals,
      "Returns the number of tasks stolen from another thread on the same locality"
                                                  );

  hpx::performance_counters::install_counter_type(
      "/workstealing/depthpool/distributedSteals",
      &getDistributedSteals,
      "Returns the number of tasks stolen from another thread on another locality"
                                                  );

  hpx::performance_counters::install_counter_type(
      "/workstealing/depthpool/localFailedSteals",
      &getFailedLocalSteals,
      "Returns the number of failed steals from this locality "
                                                  );

  hpx::performance_counters::install_counter_type(
      "/workstealing/depthpool/distributedFailedSteals",
      &getFailedDistributedSteals,
      "Returns the number of failed steals from another locality "
                                                  );
}

}

DepthPoolPolicy::DepthPoolPolicy(hpx::naming::id_type workpool) : local_workpool(workpool) {
  last_remote = hpx::find_here();

  std::random_device rd;
  randGenerator.seed(rd());
}

hpx::util::function<void(), false> DepthPoolPolicy::getWork() {
  std::unique_lock<mutex_t> l(mtx);

  hpx::util::function<void(hpx::naming::id_type)> task;
  task = hpx::async<workstealing::DepthPool::getLocal_action>(local_workpool).get();

  if (task) {
    DepthPoolPolicyPerf::perf_localSteals++;
    return hpx::util::bind(task, hpx::find_here());
  } else {
    DepthPoolPolicyPerf::perf_failedLocalSteals++;
  }

  if (!distributed_workpools.empty()) {
    // Last steal optimisation
    if (last_remote != hpx::find_here()) {
      task = hpx::async<workstealing::DepthPool::steal_action>(last_remote).get();
      if (task) {
        DepthPoolPolicyPerf::perf_distributedSteals++;
        return hpx::util::bind(task, hpx::find_here());
      } else {
        DepthPoolPolicyPerf::perf_failedDistributedSteals++;
        last_remote = hpx::find_here();
      }
    }

    // If we fail the last steal then we try else where
    std::uniform_int_distribution<int> rand(0, distributed_workpools.size() - 1);
    auto victim = distributed_workpools.begin();
    std::advance(victim, rand(randGenerator));
    task = hpx::async<workstealing::DepthPool::steal_action>(*victim).get();

    if (task) {
      last_remote = *victim;
      DepthPoolPolicyPerf::perf_distributedSteals++;
      return hpx::util::bind(task, hpx::find_here());
    } else {
      DepthPoolPolicyPerf::perf_failedDistributedSteals++;
    }
  }

  return nullptr;
}

void DepthPoolPolicy::addwork(hpx::util::function<void(hpx::naming::id_type)> task, unsigned depth) {
  std::unique_lock<mutex_t> l(mtx);
  hpx::apply<workstealing::DepthPool::addWork_action>(local_workpool, task, depth);
}

void DepthPoolPolicy::registerDistributedDepthPools(std::vector<hpx::naming::id_type> workpools) {
  std::unique_lock<mutex_t> l(mtx);
  distributed_workpools = workpools;
  distributed_workpools .erase(
      std::remove_if(distributed_workpools.begin(), distributed_workpools.end(), YewPar::util::isColocated),
      distributed_workpools.end());
}

}}
