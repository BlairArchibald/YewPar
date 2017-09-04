#include "NodeStackScheduler.hpp"
#include <memory>                                              // for __shar...
#include <ratio>                                               // for ratio
#include <utility>                                             // for pair
#include "hpx/apply.hpp"                                       // for apply
#include "hpx/lcos/async.hpp"                                  // for async
#include "hpx/lcos/detail/future_data.hpp"                     // for task_b...
#include "hpx/lcos/detail/promise_lco.hpp"                     // for promis...
#include "hpx/lcos/future.hpp"                                 // for future
#include "hpx/performance_counters/manage_counter_type.hpp"    // for instal...
#include "hpx/runtime/find_here.hpp"                           // for find_here
#include "hpx/runtime/get_colocation_id.hpp"                   // for get_co...
#include "hpx/runtime/get_os_thread_count.hpp"                 // for get_os...
#include "hpx/runtime/naming/id_type_impl.hpp"                 // for operat...
#include "hpx/runtime/naming/name.hpp"                         // for gid_type
#include "hpx/runtime/serialization/binary_filter.hpp"         // for binary...
#include "hpx/runtime/threads/thread_data.hpp"                 // for intrus...
#include "hpx/runtime/threads/thread_data_fwd.hpp"             // for get_se...
#include "hpx/runtime/threads/thread_enums.hpp"                // for thread...
#include "hpx/runtime/threads/thread_helpers.hpp"              // for suspend
#include "hpx/util/unused.hpp"                                 // for unused...
#include "hpx/lcos/base_lco_with_value.hpp"

namespace workstealing { namespace NodeStackSched {

void stopScheduler() {
  running.store(false);
  tasks_required_sem.signal(1);
}

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
      "/workstealing/NodeStack/steals",
      &getSteals,
      "Returns the number of tasks stolen from another thread"
                                                  );
  hpx::performance_counters::install_counter_type(
      "/workstealing/NodeStack/failedsteals",
      &getFailedSteals,
      "Returns the number of failed steals from another thread"
  );
}
}}
