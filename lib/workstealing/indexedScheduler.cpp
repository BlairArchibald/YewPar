#include "indexedScheduler.hpp"
#include <memory>                                              // for __shar...
#include <ratio>                                               // for ratio
#include <utility>                                             // for pair
#include "ExponentialBackoff.hpp"                              // for Expone...
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
#include "hpx/runtime/threads/executors/current_executor.hpp"  // for curren...
#include "hpx/runtime/threads/executors/default_executor.hpp"  // for defaul...
#include "hpx/runtime/threads/thread_data.hpp"                 // for intrus...
#include "hpx/runtime/threads/thread_data_fwd.hpp"             // for get_se...
#include "hpx/runtime/threads/thread_enums.hpp"                // for thread...
#include "hpx/runtime/threads/thread_helpers.hpp"              // for suspend
#include "hpx/util/unused.hpp"                                 // for unused...
#include "hpx/lcos/base_lco_with_value.hpp"
#include "posManager.hpp"                                      // for posMan...

namespace workstealing { namespace indexed {

void stopScheduler() {
  running.store(false);
  tasks_required_sem.signal(1);
}

void startScheduler(std::vector<hpx::naming::id_type> posManagers) {
  auto schedulerReady = std::make_shared<hpx::promise<void> >();

  if (hpx::get_os_thread_count() > 1) {
    hpx::threads::executors::default_executor high_priority_executor(hpx::threads::thread_priority_critical,
                                                                     hpx::threads::thread_stacksize_large);
    hpx::apply(high_priority_executor, &scheduler, posManagers, schedulerReady);
  } else {
    hpx::threads::executors::default_executor exe(hpx::threads::thread_stacksize_large);
    hpx::apply(exe, &scheduler, posManagers, schedulerReady);
  }

  schedulerReady->get_future().get();
}

void scheduler(std::vector<hpx::naming::id_type> posManagers, std::shared_ptr<hpx::promise<void> >readyPromise) {
  auto here = hpx::find_here();
  hpx::naming::id_type posManager;

  // Find the local posManager
  for (auto it = posManagers.begin(); it != posManagers.end(); ++it) {
    if (hpx::get_colocation_id(*it).get() == here) {
      posManager = *it;
      posManagers.erase(it);
      break;
    }
  }

  // Register all other posManagers with the local manager
  hpx::async<workstealing::indexed::posManager::registerDistributedManagers_action>(posManager, posManagers);

  // posManager variables are set up, we can start generating tasks
  readyPromise->set_value();

  auto threads = hpx::get_os_thread_count();
  hpx::threads::executors::current_executor scheduler;

  // Pre-init the sem
  if (threads > 1) {
    // Don't both scheduling in the one thread case since we never need more
    // work in the indexed scheme
    // FIXME: Why not just return?
    tasks_required_sem.signal(threads - 1);
  }

  ExponentialBackoff backoff;

  while (running) {
    tasks_required_sem.wait();

    auto start_time = std::chrono::steady_clock::now();
    auto scheduled = hpx::async<workstealing::indexed::posManager::getWork_action>(posManager).get();
    auto overall_time = std::chrono::duration_cast<std::chrono::milliseconds>
                        (std::chrono::steady_clock::now() - start_time);
    std::cout << "scheduling cpu = " << overall_time.count() << std::endl;
    if (scheduled) {
      perf_steals++;
      backoff.reset();
    } else {
      perf_failedSteals++;

      backoff.failed();
      hpx::this_thread::suspend(backoff.getSleepTime());

      tasks_required_sem.signal();
    }
  }
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
      "/workstealing/indexed/steals",
      &getSteals,
      "Returns the number of tasks stolen from another thread"
                                                  );
  hpx::performance_counters::install_counter_type(
      "/workstealing/indexed/failedsteals",
      &getFailedSteals,
      "Returns the number of failed steals from another thread"
  );
}
}}
