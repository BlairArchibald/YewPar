#include "priorityscheduler.hpp"
#include "priorityworkqueue.hpp"

#include "ExponentialBackoff.hpp"

namespace workstealing { namespace priority {

void stopScheduler() {
  running.store(false);
  tasks_required_sem.signal(1);
}

void startScheduler(hpx::naming::id_type globalWorkqueue) {
  workstealing::priority::global_workqueue = globalWorkqueue;

  if (hpx::get_os_thread_count() > 1) {
    hpx::threads::executors::default_executor high_priority_executor(hpx::threads::thread_priority_critical);
    hpx::apply(high_priority_executor, &scheduler);
  } else {
    hpx::apply(&scheduler);
  }
}

void scheduler() {
  auto threads = hpx::get_os_thread_count() == 1 ? 1 : hpx::get_os_thread_count() - 1;
  hpx::threads::executors::current_executor scheduler;

  // Pre-init the sem
  tasks_required_sem.signal(threads);

  ExponentialBackoff backoff;

  while (running) {
    tasks_required_sem.wait();

    // Try local queue first then distributed
    hpx::util::function<void(hpx::naming::id_type)> task;
    task = hpx::async<workstealing::priorityworkqueue::steal_action>(global_workqueue).get();
    if (task) {
      perf_steals++;
      scheduler.add(hpx::util::bind(task, hpx::find_here()));
      backoff.reset();
    } else {
      perf_failedSteals++;

      backoff.failed();
      hpx::this_thread::suspend(backoff.getSleepTime());

      tasks_required_sem.signal();
    }
  }
}

// FIXME: Abstract these counter functions into a single function parameterised
// by the atomic counter (couldn't get this idea to compile correctly)
std::int64_t getSteals(bool reset) {
  auto res = perf_steals.load();
  if (reset) {
    perf_steals.store(0);
  }
  return res;
}

std::int64_t getFailedSteals(bool reset) {
  auto res = perf_failedSteals.load();
  if (reset) {
    perf_failedSteals.store(0);
  }
  return res;
}

void registerPerformanceCounters() {
  hpx::performance_counters::install_counter_type(
    "/workstealing/priority/steals",
    &getSteals,
    "Returns the number of tasks converted from the global priority workqueue"
    );
  hpx::performance_counters::install_counter_type(
    "/workstealing/priority/failedsteals",
    &getFailedSteals,
    "Returns the number of failed steals from the global priority workqueue"
    );
}

}}
