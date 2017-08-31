#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <atomic>
#include <memory>

#include <hpx/hpx.hpp>

namespace workstealing {
  std::atomic<bool> running(true);
  hpx::lcos::local::counting_semaphore tasks_required_sem;
  hpx::naming::id_type local_workqueue;

  void startScheduler(std::vector<hpx::naming::id_type> workqueues);
  void stopScheduler();

  void scheduler(std::vector<hpx::naming::id_type> workqueues, std::shared_ptr<hpx::promise<void> >readyPromise);

  // Performance Counters
  std::atomic<std::int64_t> perf_localSteals(0);
  std::atomic<std::int64_t> perf_distSteals(0);
  std::atomic<std::int64_t> perf_failedSteals(0);
  void registerPerformanceCounters();
}

HPX_PLAIN_ACTION(workstealing::stopScheduler, stopScheduler_action);
HPX_PLAIN_ACTION(workstealing::startScheduler, startScheduler_action);

#endif
