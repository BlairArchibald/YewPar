#ifndef YEWPAR_POLICY_PRIORITYORDERED_HPP
#define YEWPAR_POLICY_PRIORITYORDERED_HPP

#include <hpx/modules/collectives.hpp>
#include <hpx/runtime_distributed/find_all_localities.hpp>

#include "Policy.hpp"
#include "workstealing/PriorityWorkqueue.hpp"

namespace Workstealing { namespace Scheduler {extern std::shared_ptr<Policy> local_policy; }}

namespace Workstealing { namespace Policies {

namespace PriorityOrderedPerf {

extern std::atomic<std::uint64_t> perf_spawns;
extern std::atomic<std::uint64_t> perf_steals;
extern std::atomic<std::uint64_t> perf_failedSteals;

void registerPerformanceCounters();

}


class PriorityOrderedPolicy : public Policy {
 private:
  hpx::id_type globalWorkqueue;

  using mutex_t = hpx::mutex;
  mutex_t mtx;

 public:
  PriorityOrderedPolicy (hpx::id_type gWorkqueue) : globalWorkqueue(gWorkqueue) {};

  // Priority Ordered policy just forwards requests to the global workqueue
  hpx::function<void(), false> getWork() override {
    std::unique_lock<mutex_t> l(mtx);

    hpx::distributed::function<void(hpx::id_type)> task;
    task = hpx::async<workstealing::PriorityWorkqueue::steal_action>(globalWorkqueue).get();
    if (task) {
      PriorityOrderedPerf::perf_steals++;
      return hpx::bind(task, hpx::find_here());
    }

    PriorityOrderedPerf::perf_failedSteals++;
    return nullptr;
  }

  void addwork(int priority, hpx::distributed::function<void(hpx::id_type)> task) {
    std::unique_lock<mutex_t> l(mtx);
    PriorityOrderedPerf::perf_spawns++;
    hpx::apply<workstealing::PriorityWorkqueue::addWork_action>(globalWorkqueue, priority, task);
  }

  hpx::future<bool> workRemaining() {
    std::unique_lock<mutex_t> l(mtx);
    return hpx::async<workstealing::PriorityWorkqueue::workRemaining_action>(globalWorkqueue);
  }

  // Policy initialiser
  static void setPriorityWorkqueuePolicy(hpx::id_type globalWorkqueue) {
    Workstealing::Scheduler::local_policy = std::make_shared<PriorityOrderedPolicy>(globalWorkqueue);
  }
  struct setPriorityWorkqueuePolicy_act : hpx::actions::make_action<
    decltype(&PriorityOrderedPolicy::setPriorityWorkqueuePolicy),
    &PriorityOrderedPolicy::setPriorityWorkqueuePolicy,
    setPriorityWorkqueuePolicy_act>::type {};

  static void initPolicy () {
    auto globalWorkqueue = hpx::new_<workstealing::PriorityWorkqueue>(hpx::find_here()).get();
    hpx::wait_all(hpx::lcos::broadcast<setPriorityWorkqueuePolicy_act>(
        hpx::find_all_localities(), globalWorkqueue));
  }
};

}}

#endif
