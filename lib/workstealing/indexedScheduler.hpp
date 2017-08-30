#ifndef SCHEDULER_INDEXED_HPP
#define SCHEDULER_INDEXED_HPP

#include <atomic>
#include <memory>
#include <cstdint>

#include <hpx/lcos/promise.hpp>
#include <hpx/lcos/local/counting_semaphore.hpp>
#include <hpx/runtime/actions/plain_action.hpp>
#include <hpx/runtime/naming/id_type.hpp>

namespace workstealing { namespace indexed {
  std::atomic<bool> running(true);
  hpx::lcos::local::counting_semaphore tasks_required_sem;
  hpx::naming::id_type local_workqueue;

  void startScheduler(std::vector<hpx::naming::id_type> posManagers);
  void stopScheduler();
  void scheduler(std::vector<hpx::naming::id_type> posManagers, std::shared_ptr<hpx::promise<void> >readyPromise);

  std::atomic<std::uint64_t> perf_steals(0);
  std::atomic<std::uint64_t> perf_failedSteals(0);
  void registerPerformanceCounters();
}}

HPX_PLAIN_ACTION(workstealing::indexed::stopScheduler, stopScheduler_indexed_action);
HPX_PLAIN_ACTION(workstealing::indexed::startScheduler, startScheduler_indexed_action);

#endif
