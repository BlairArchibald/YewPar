#ifndef SCHEDULER_INDEXED_HPP
#define SCHEDULER_INDEXED_HPP

#include <atomic>
#include <memory>

#include <hpx/hpx.hpp>

namespace workstealing { namespace indexed {
  std::atomic<bool> running(true);
  hpx::lcos::local::counting_semaphore tasks_required_sem;
  hpx::naming::id_type local_workqueue;

  void startScheduler(std::vector<hpx::naming::id_type> posManagers);
  void stopScheduler();
  void scheduler(std::vector<hpx::naming::id_type> posManagers, std::shared_ptr<hpx::promise<void> >readyPromise);
}}

HPX_PLAIN_ACTION(workstealing::indexed::stopScheduler, stopScheduler_indexed_action);
HPX_PLAIN_ACTION(workstealing::indexed::startScheduler, startScheduler_indexed_action);

#endif
