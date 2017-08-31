#ifndef PRIORITY_SCHEDULER_HPP
#define PRIORITY_SCHEDULER_HPP

#include <stdint.h>                                              // for int64_t
#include <atomic>                                                // for atomic
#include "hpx/lcos/local/counting_semaphore.hpp"                 // for coun...
#include "hpx/runtime/actions/basic_action.hpp"                  // for HPX_...
#include "hpx/runtime/actions/plain_action.hpp"                  // for HPX_...
#include "hpx/runtime/actions/transfer_action.hpp"               // for tran...
#include "hpx/runtime/actions/transfer_continuation_action.hpp"  // for tran...
#include "hpx/runtime/naming/id_type.hpp"                        // for id_type
#include "hpx/traits/is_action.hpp"                              // for is_a...
#include "hpx/traits/needs_automatic_registration.hpp"           // for need...

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
