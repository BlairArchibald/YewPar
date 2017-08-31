#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <stdint.h>                                              // for int64_t
#include <atomic>                                                // for atomic
#include <vector>                                                // for vector
#include "hpx/lcos/local/counting_semaphore.hpp"                 // for coun...
#include "hpx/lcos/promise.hpp"                                  // for promise
#include "hpx/runtime/actions/basic_action.hpp"                  // for HPX_...
#include "hpx/runtime/actions/plain_action.hpp"                  // for HPX_...
#include "hpx/runtime/actions/transfer_action.hpp"               // for tran...
#include "hpx/runtime/actions/transfer_continuation_action.hpp"  // for tran...
#include "hpx/runtime/naming/id_type.hpp"                        // for id_type
#include "hpx/traits/is_action.hpp"                              // for is_a...
#include "hpx/traits/needs_automatic_registration.hpp"           // for need...

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
