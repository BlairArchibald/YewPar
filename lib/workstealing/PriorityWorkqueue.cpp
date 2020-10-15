#include "PriorityWorkqueue.hpp"

namespace workstealing {

using funcType = hpx::util::function<void(hpx::naming::id_type)>;
funcType PriorityWorkqueue::steal() {
  if (!tasks.empty()) {
    auto task = tasks.top();
    tasks.pop();
    return hpx::util::get<1>(task);
  }
  return nullptr;
}

void PriorityWorkqueue::addWork(int priority, funcType task) {
  tasks.push(hpx::util::make_tuple(priority, std::move(task)));
}

bool PriorityWorkqueue::workRemaining() {
  return tasks.empty();
}
}
HPX_REGISTER_COMPONENT_MODULE();

typedef hpx::components::component<workstealing::PriorityWorkqueue> workqueue_type;

HPX_REGISTER_COMPONENT(workqueue_type, priority_workqueue);

HPX_REGISTER_ACTION(workstealing::PriorityWorkqueue::steal_action, workqueue_prio_steal_action);
HPX_REGISTER_ACTION(workstealing::PriorityWorkqueue::addWork_action, workqueue_prio_addWork_action);
HPX_REGISTER_ACTION(workstealing::PriorityWorkqueue::workRemaining_action, workqueue_prio_workRemaining_action);
