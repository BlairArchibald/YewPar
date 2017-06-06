#ifndef PRIORITY_SCHEDULER_HPP
#define PRIORITY_SCHEDULER_HPP

#include <atomic>
#include <memory>

#include <hpx/hpx.hpp>

namespace workstealing { namespace priority {
  std::atomic<bool> running(true);
  hpx::lcos::local::counting_semaphore tasks_required_sem;
  hpx::naming::id_type global_workqueue;

  void startScheduler(hpx::naming::id_type globalWorkqueue);
  void stopScheduler();

  void scheduler();

  // Performance Counters
  std::atomic<std::int64_t> perf_steals(0);
  std::atomic<std::int64_t> perf_failedSteals(0);
  void registerPerformanceCounters();
}}

HPX_PLAIN_ACTION(workstealing::priority::stopScheduler, priority_stopScheduler_action);
HPX_PLAIN_ACTION(workstealing::priority::startScheduler, priority_startScheduler_action);

#endif
