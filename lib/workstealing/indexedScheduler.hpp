#ifndef SCHEDULER_INDEXED_HPP
#define SCHEDULER_INDEXED_HPP

#include <atomic>
#include <memory>

#include <hpx/hpx.hpp>

namespace workstealing { namespace indexed {
  std::atomic<bool> running(true);
  hpx::lcos::local::counting_semaphore tasks_required_sem;
  hpx::naming::id_type local_workqueue;

  template <typename Fn>
  void startScheduler(hpx::naming::id_type posManager);
  void stopScheduler();

  template <typename Fn>
  void scheduler(hpx::naming::id_type posManager, std::shared_ptr<hpx::promise<void> >readyPromise);
}}

HPX_PLAIN_ACTION(workstealing::indexed::stopScheduler, stopScheduler_indexed_action);

#endif
