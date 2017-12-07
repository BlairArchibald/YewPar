#ifndef YEWPAR_POLICY_PRIORITYORDERED_HPP
#define YEWPAR_POLICY_WORKPOOL_HPP

#include "Policy.hpp"
#include "workstealing/priorityworkqueue.hpp"

namespace Workstealing { namespace Policies {

class PriorityOrderedPolicy : public Policy {
 private:
  hpx::naming::id_type globalWorkqueue;

  using mutex_t = hpx::lcos::local::mutex;
  mutex_t mtx;

 public:
  PriorityOrderedPolicy (hpx::naming::id_type gWorkqueue) : globalWorkqueue(gWorkqueue) {};

  // Priority Ordered policy just forwards requests to the global workqueue
  hpx::util::function<void(), false> getWork() override {
    std::unique_lock<mutex_t> l(mtx);

    hpx::util::function<void(hpx::naming::id_type)> task;
    task = hpx::async<workstealing::priorityworkqueue::steal_action>(globalWorkqueue).get();
    return hpx::util::bind(task, hpx::find_here());
  }

  void addwork(int priority, hpx::util::function<void(hpx::naming::id_type)> task) {
    std::unique_lock<mutex_t> l(mtx);
    hpx::apply<workstealing::priorityworkqueue::addWork_action>(globalWorkqueue, priority, task);
  }

  // Policy initialiser
  static void setPriorityWorkqueuePolicy(hpx::naming::id_type globalWorkqueue) {
    Workstealing::Scheduler::local_policy = std::make_shared<PriorityOrderedPolicy>(globalWorkqueue);
  }
  struct setPriorityWorkqueuePolicy_act : hpx::actions::make_action<
    decltype(&PriorityOrderedPolicy::setPriorityWorkqueuePolicy),
    &PriorityOrderedPolicy::setPriorityWorkqueuePolicy,
    setPriorityWorkqueuePolicy_act>::type {};

  static void initPolicy () {
    auto globalWorkqueue = hpx::new_<workstealing::priorityworkqueue>(hpx::find_here()).get();
  }
};

}}

#endif
