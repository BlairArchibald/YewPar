#ifndef SCHEDULER_NODESTACK_HPP
#define SCHEDULER_NODESTACK_HPP

#include <atomic>                                                // for atomic
#include <cstdint>                                               // for uint...
#include <vector>                                                // for vector
#include <memory>                                                // for shared_ptr
#include "hpx/lcos/local/counting_semaphore.hpp"                 // for coun...
#include "hpx/lcos/promise.hpp"                                  // for promise
#include "hpx/runtime/actions/basic_action.hpp"                  // for HPX_...
#include "hpx/runtime/actions/plain_action.hpp"                  // for HPX_...
#include "hpx/runtime/actions/transfer_action.hpp"               // for tran...
#include "hpx/runtime/actions/transfer_continuation_action.hpp"  // for tran...
#include "hpx/runtime/naming/id_type.hpp"                        // for id_type
#include "hpx/traits/is_action.hpp"                              // for is_a...
#include "hpx/traits/needs_automatic_registration.hpp"           // for need...
#include "hpx/runtime/threads/executors/default_executor.hpp"  // for defaul...
#include "hpx/runtime/threads/executors/current_executor.hpp"  // for curren...
#include "hpx/runtime/find_here.hpp"                           // for find_here
#include "ExponentialBackoff.hpp"                              // for Expone...
#include "NodeStackManager.hpp"

namespace workstealing { namespace NodeStackSched {
std::atomic<bool> running(true);
hpx::lcos::local::counting_semaphore tasks_required_sem;

void stopScheduler();

std::atomic<std::uint64_t> perf_steals(0);
std::atomic<std::uint64_t> perf_failedSteals(0);
void registerPerformanceCounters();

template <typename Sol, typename ChildFunc>
void scheduler(std::vector<hpx::naming::id_type> nodeStackMgrs, std::shared_ptr<hpx::promise<void> >readyPromise) {
  auto here = hpx::find_here();
  hpx::naming::id_type nodeStackMgr;

  // Find the local posManager
  for (auto it = nodeStackMgrs.begin(); it != nodeStackMgrs.end(); ++it) {
    if (hpx::get_colocation_id(*it).get() == here) {
      nodeStackMgr = *it;
      nodeStackMgrs.erase(it);
      break;
    }
  }

  // Register all other posManagers with the local manager
  typedef typename NodeStackManager<Sol, ChildFunc>::registerDistributedManagers_action regDistMgrsAct;
  hpx::async<regDistMgrsAct>(nodeStackMgr, nodeStackMgrs);

  // posManager variables are set up, we can start generating tasks
  readyPromise->set_value();

  auto threads = hpx::get_os_thread_count();
  hpx::threads::executors::current_executor scheduler;

  // Pre-init the sem
  if (threads == 0) {
    return;
  }

  tasks_required_sem.signal(threads - 1);

  ExponentialBackoff backoff;

  for (;;) {
    tasks_required_sem.wait();

    if (!running) {
      break;
    }

    typedef typename NodeStackManager<Sol, ChildFunc>::getWork_action getWorkAct;
    auto scheduled = hpx::async<getWorkAct>(nodeStackMgr).get();
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

template <typename Sol, typename ChildFunc>
void startScheduler(std::vector<hpx::naming::id_type> nodeStackMgrs) {
  auto schedulerReady = std::make_shared<hpx::promise<void> >();

  if (hpx::get_os_thread_count() > 1) {
    hpx::threads::executors::default_executor high_priority_executor(hpx::threads::thread_priority_critical,
                                                                     hpx::threads::thread_stacksize_large);
    hpx::apply(high_priority_executor, &scheduler<Sol, ChildFunc>, nodeStackMgrs, schedulerReady);
  } else {
    hpx::threads::executors::default_executor exe(hpx::threads::thread_stacksize_large);
    hpx::apply(exe, &scheduler<Sol, ChildFunc>, nodeStackMgrs, schedulerReady);
  }

  schedulerReady->get_future().get();
}

template <typename Sol, typename ChildFunc>
struct startSchedulerAct : hpx::actions::make_action<
  decltype(&startScheduler<Sol, ChildFunc>),
      &startScheduler<Sol, ChildFunc>,
      startSchedulerAct<Sol, ChildFunc> >::type {};
}}

HPX_PLAIN_ACTION(workstealing::NodeStackSched::stopScheduler, stopScheduler_NodeStack_action);

#endif
