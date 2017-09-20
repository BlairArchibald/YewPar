#ifndef SCHEDULER_SEARCHMANAGER_HPP
#define SCHEDULER_SEARCHMANAGER_HPP

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
#include "SearchManager.hpp"

namespace workstealing { namespace SearchManagerSched {

std::atomic<bool> running(true);
hpx::lcos::local::counting_semaphore tasks_required_sem;

void stopScheduler();

std::atomic<std::uint64_t> perf_steals(0);
std::atomic<std::uint64_t> perf_failedSteals(0);
void registerPerformanceCounters();

template <typename SearchInfo, typename FuncToCall>
void scheduler(std::vector<hpx::naming::id_type> searchManagers, std::shared_ptr<hpx::promise<void> >readyPromise) {
  hpx::naming::id_type localSearchManager;

  // Find the local searchManager. Remove it from the list of distributed searchManagers
  for (auto it = searchManagers.begin(); it != searchManagers.end(); ++it) {
    if (hpx::get_colocation_id(*it).get() == hpx::find_here()) {
      localSearchManager = *it;
      searchManagers.erase(it);
      break;
    }
  }

  typedef typename SearchManager<SearchInfo, FuncToCall>::registerDistributedManagers_action regDistMgrsAct;
  hpx::async<regDistMgrsAct>(localSearchManager, searchManagers);

  readyPromise->set_value();

  auto threads = hpx::get_os_thread_count();
  hpx::threads::executors::current_executor scheduler;

  // FIXME: Currently assumes one thread per locality is initialised with work via SearchManager::addWork(..)
  if (threads == 1) {
    return;
  }
  tasks_required_sem.signal(threads - 1);

  ExponentialBackoff backoff;
  for (;;) {
    tasks_required_sem.wait();

    if (!running) {
      break;
    }

    typedef typename SearchManager<SearchInfo, FuncToCall>::getWork_action getWorkAct;
    auto scheduled = hpx::async<getWorkAct>(localSearchManager).get();
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

template <typename SearchInfo, typename FuncToCall>
void startScheduler(std::vector<hpx::naming::id_type> searchManagers) {
  auto schedulerReady = std::make_shared<hpx::promise<void> >();

  if (hpx::get_os_thread_count() > 1) {
    hpx::threads::executors::default_executor high_priority_executor(hpx::threads::thread_priority_critical,
                                                                     hpx::threads::thread_stacksize_large);
    hpx::apply(high_priority_executor, &scheduler<SearchInfo, FuncToCall>, searchManagers, schedulerReady);
  } else {
    hpx::threads::executors::default_executor exe(hpx::threads::thread_stacksize_large);
    hpx::apply(exe, &scheduler<SearchInfo, FuncToCall>, searchManagers, schedulerReady);
  }

  schedulerReady->get_future().get();
}

template <typename SearchInfo, typename FuncToCall>
struct startSchedulerAct : hpx::actions::make_action<
  decltype(&startScheduler<SearchInfo, FuncToCall>),
  &startScheduler<SearchInfo, FuncToCall>,
  startSchedulerAct<SearchInfo, FuncToCall> >::type {};

}} // namespace workstealing // namespace SearchManagerSched

HPX_PLAIN_ACTION(workstealing::SearchManagerSched::stopScheduler, stopScheduler_SearchManager_action);

#endif //SCHEDULER_SEARCHMANAGER_HPP
