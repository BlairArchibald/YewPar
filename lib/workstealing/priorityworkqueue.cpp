#include "priorityworkqueue.hpp"

namespace workstealing
{
  using funcType = hpx::util::function<void(hpx::naming::id_type)>;
  funcType priorityworkqueue::steal() {
    if (!tasks.empty()) {
      auto task = tasks.top();
      tasks.pop();
      return hpx::util::get<1>(task);
    }
    return nullptr;
  }

  void priorityworkqueue::addWork(int priority, funcType task) {
    tasks.push(hpx::util::make_tuple(priority, std::move(task)));
  }
}
HPX_REGISTER_COMPONENT_MODULE();

typedef hpx::components::component<workstealing::priorityworkqueue> workqueue_type;

HPX_REGISTER_COMPONENT(workqueue_type, priority_workqueue);

HPX_REGISTER_ACTION(workstealing::priorityworkqueue::steal_action, workqueue_prio_steal_action);
HPX_REGISTER_ACTION(workstealing::priorityworkqueue::addWork_action, workqueue_prio_addWork_action);
