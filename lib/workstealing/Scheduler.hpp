#ifndef YEWPAR_SCHEDULER_HPP
#define YEWPAR_SCHEDULER_HPP

#include <atomic>

#include <hpx/modules/actions_base.hpp>
#include <hpx/synchronization/mutex.hpp>
#include <hpx/synchronization/condition_variable.hpp>

#include "policies/Policy.hpp"

namespace Workstealing { namespace Scheduler {

// Used to stop all schedulers
std::atomic<bool> running(true);

hpx::mutex mtx;
hpx::condition_variable exit_cv;
unsigned numRunningSchedulers;

// Implementation policy
std::shared_ptr<Policy> local_policy;

void stopSchedulers();
HPX_DEFINE_PLAIN_ACTION(stopSchedulers, stopSchedulers_act);

void scheduler(hpx::function<void(), false> initialTask);

// Start "n" uninitialised schedulers
void startSchedulers(unsigned n);
HPX_DEFINE_PLAIN_ACTION(startSchedulers, startSchedulers_act);

}} // Workstealing::Scheduler


#endif //YEWPAR_SCHEDULER_HPP
