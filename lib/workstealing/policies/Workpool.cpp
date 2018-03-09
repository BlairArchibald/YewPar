#include "Workpool.hpp"

#include <hpx/util/function.hpp>
#include <hpx/runtime/find_here.hpp>
#include <hpx/performance_counters/manage_counter_type.hpp>

#include <memory>

#include "util/util.hpp"

namespace Workstealing { namespace Policies {

namespace WorkpoolPerf {

std::atomic<std::uint64_t> perf_spawns(0);
std::atomic<std::uint64_t> perf_localSteals(0);
std::atomic<std::uint64_t> perf_distributedSteals(0);
std::atomic<std::uint64_t> perf_failedLocalSteals(0);
std::atomic<std::uint64_t> perf_failedDistributedSteals(0);

std::uint64_t get_and_reset(std::atomic<std::uint64_t> & cntr, bool reset) {
  auto res = cntr.load();
  if (reset) { cntr = 0; }
  return res;
}

std::uint64_t getSpawns (bool reset) {get_and_reset(perf_spawns, reset);}
std::uint64_t getLocalSteals(bool reset) {get_and_reset(perf_localSteals, reset);}
std::uint64_t getDistributedSteals (bool reset) {get_and_reset(perf_distributedSteals, reset);}
std::uint64_t getFailedLocalSteals(bool reset) {get_and_reset(perf_failedLocalSteals, reset);}
std::uint64_t getFailedDistributedSteals(bool reset) {get_and_reset(perf_failedDistributedSteals, reset);}

void registerPerformanceCounters() {
  hpx::performance_counters::install_counter_type(
      "/workstealing/Workpool/spawns",
      &getSpawns,
      "Returns the number of tasks spawned on this locality"
                                                  );

  hpx::performance_counters::install_counter_type(
      "/workstealing/Workpool/localSteals",
      &getLocalSteals,
      "Returns the number of tasks stolen from another thread on the same locality"
                                                  );

  hpx::performance_counters::install_counter_type(
      "/workstealing/Workpool/distributedSteals",
      &getDistributedSteals,
      "Returns the number of tasks stolen from another thread on another locality"
                                                  );

  hpx::performance_counters::install_counter_type(
      "/workstealing/Workpool/localFailedSteals",
      &getFailedLocalSteals,
      "Returns the number of failed steals from this locality "
                                                  );

  hpx::performance_counters::install_counter_type(
      "/workstealing/Workpool/distributedFailedSteals",
      &getFailedDistributedSteals,
      "Returns the number of failed steals from another locality "
                                                  );

}

}

Workpool::Workpool(hpx::naming::id_type localQueue) {
  local_workqueue = localQueue;
  last_remote = hpx::find_here();

  std::random_device rd;
  randGenerator.seed(rd());
}

hpx::util::function<void(), false> Workpool::getWork() {
  std::unique_lock<mutex_t> l(mtx);

  hpx::util::function<void(hpx::naming::id_type)> task;
  task = hpx::async<workstealing::workqueue::getLocal_action>(local_workqueue).get();

  if (task) {
    WorkpoolPerf::perf_localSteals++;
    return hpx::util::bind(task, hpx::find_here());
  } else {
    WorkpoolPerf::perf_failedLocalSteals++;
  }

  if (!distributed_workqueues.empty()) {
    // Last steal optimisation
    if (last_remote != hpx::find_here()) {
      task = hpx::async<workstealing::workqueue::steal_action>(last_remote).get();
      if (task) {
        WorkpoolPerf::perf_distributedSteals++;
        return hpx::util::bind(task, hpx::find_here());
      } else {
        WorkpoolPerf::perf_failedDistributedSteals++;
        last_remote = hpx::find_here();
      }
    }

    // If we fail the last steal then we try else where
    std::uniform_int_distribution<int> rand(0, distributed_workqueues.size() - 1);
    auto victim = distributed_workqueues.begin();
    std::advance(victim, rand(randGenerator));
    task = hpx::async<workstealing::workqueue::steal_action>(*victim).get();

    if (task) {
      last_remote = *victim;
      WorkpoolPerf::perf_distributedSteals++;
      return hpx::util::bind(task, hpx::find_here());
    } else {
      WorkpoolPerf::perf_failedDistributedSteals++;
    }
  }

  return nullptr;
}

void Workpool::addwork(hpx::util::function<void(hpx::naming::id_type)> task) {
  std::unique_lock<mutex_t> l(mtx);
  WorkpoolPerf::perf_spawns++;
  hpx::apply<workstealing::workqueue::addWork_action>(local_workqueue, task);
}

void Workpool::registerDistributedWorkqueues(std::vector<hpx::naming::id_type> workqueues) {
  std::unique_lock<mutex_t> l(mtx);
  distributed_workqueues = workqueues;
  distributed_workqueues.erase(
      std::remove_if(distributed_workqueues.begin(), distributed_workqueues.end(), YewPar::util::isColocated),
      distributed_workqueues.end());
}

}}
