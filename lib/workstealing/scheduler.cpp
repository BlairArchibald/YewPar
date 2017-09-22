#include "scheduler.hpp"
#include <iterator>                                            // for advance
#include <memory>                                              // for __shar...
#include <random>                                              // for defaul...
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
#include "hpx/runtime/find_localities.hpp"                     // for find_a...
#include "hpx/runtime/get_colocation_id.hpp"                   // for get_co...
#include "hpx/runtime/get_os_thread_count.hpp"                 // for get_os...
#include "hpx/runtime/naming/id_type_impl.hpp"                 // for operat...
#include "hpx/runtime/naming/name.hpp"                         // for intrus...
#include "hpx/runtime/serialization/binary_filter.hpp"         // for binary...
#include "hpx/runtime/serialization/serialize.hpp"             // for operat...
#include "hpx/runtime/threads/executors/current_executor.hpp"  // for curren...
#include "hpx/runtime/threads/executors/default_executor.hpp"  // for defaul...
#include "hpx/runtime/threads/thread_data_fwd.hpp"             // for get_se...
#include "hpx/runtime/threads/thread_enums.hpp"                // for thread...
#include "hpx/runtime/threads/thread_helpers.hpp"              // for suspend
#include "hpx/util/bind.hpp"                                   // for bound
#include "hpx/util/function.hpp"                               // for function
#include "hpx/util/unused.hpp"                                 // for unused...
#include "workqueue.hpp"                                       // for workqu...

namespace workstealing {

void stopScheduler() {
  running.store(false);
  tasks_required_sem.signal(1);
}

void startScheduler(std::vector<hpx::naming::id_type> workqueues) {
  auto schedulerReady = std::make_shared<hpx::promise<void> >();

  if (hpx::get_os_thread_count() > 1) {
    hpx::threads::executors::default_executor high_priority_executor(hpx::threads::thread_priority_critical,
                                                                     hpx::threads::thread_stacksize_large);
    hpx::apply(high_priority_executor, &scheduler, workqueues, schedulerReady);
  } else {
    hpx::threads::executors::default_executor exe(hpx::threads::thread_stacksize_large);
    hpx::apply(exe, &scheduler, workqueues, schedulerReady);
  }

  schedulerReady->get_future().get();
}

void scheduler(std::vector<hpx::naming::id_type> workqueues,
               std::shared_ptr<hpx::promise<void> >readyPromise) {
  auto here = hpx::find_here();
  hpx::naming::id_type last_remote = here; // We use *here* as a sentinel value

  auto distributed = hpx::find_all_localities().size() > 1;

  // Figure out which workqueue is local to this scheduler
  for (auto it = workqueues.begin(); it != workqueues.end(); ++it) {
    if (hpx::get_colocation_id(*it).get() == here) {
      local_workqueue = *it;
      workqueues.erase(it);
      break;
    }
  }

  // Workqueue variables are set up, we can start generating tasks
  readyPromise->set_value();

  auto threads = hpx::get_os_thread_count() == 1 ? 1 : hpx::get_os_thread_count() - 1;
  hpx::threads::executors::current_executor scheduler;

  // Pre-init the sem
  tasks_required_sem.signal(threads);

  // Exponential backoff
  ExponentialBackoff backoff;

  // Randomness for victim selection
  std::random_device rd;
  std::mt19937 randGenerator(rd());
  std::uniform_int_distribution<int> rand(0, workqueues.size() - 1);

  while (running) {
    tasks_required_sem.wait();

    // Try local queue first then distributed
    hpx::util::function<void(hpx::naming::id_type)> task;
    task = hpx::async<workstealing::workqueue::getLocal_action>(local_workqueue).get();
    if (task) { perf_localSteals++; }
    if (distributed && !task) {
      if (last_remote != here) {
        task = hpx::async<workstealing::workqueue::steal_action>(last_remote).get();
        if (task) {
          perf_distSteals++;
        }
      }

      if (!task) {
        auto victim = workqueues.begin();

        std::advance(victim, rand(randGenerator));

        task = hpx::async<workstealing::workqueue::steal_action>(*victim).get();
        if (task) {
          perf_distSteals++;
          last_remote = *victim;
        } else {
          last_remote = here;
        }
      }
    }

    if (task) {
      scheduler.add(hpx::util::bind(task, here),
                    "YewPar Task",
                    hpx::threads::pending,
                    true,
                    hpx::threads::thread_stacksize_huge);
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
std::int64_t getLocalSteals(bool reset) {
  auto res = perf_localSteals.load();
  if (reset) {
    perf_localSteals.store(0);
  }
  return res;
}

std::int64_t getDistSteals(bool reset) {
  auto res = perf_distSteals.load();
  if (reset) {
    perf_distSteals.store(0);
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
    "/workstealing/localsteals",
    &getLocalSteals,
    "Returns the number of tasks converted from the local workqueue"
    );
  hpx::performance_counters::install_counter_type(
    "/workstealing/diststeals",
    &getDistSteals,
    "Returns the number of tasks converted from a distributed workqueue"
    );
  hpx::performance_counters::install_counter_type(
    "/workstealing/failedsteals",
    &getFailedSteals,
    "Returns the number of failed steals"
    );
}

}
