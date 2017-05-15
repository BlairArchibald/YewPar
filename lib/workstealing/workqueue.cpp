#include "workqueue.hpp"

namespace workstealing
{
  using funcType = hpx::util::function<void(hpx::naming::id_type)>;
  funcType workqueue::steal() {
    if (tasks.size() >= 1) {
      auto task = tasks.front();
      tasks.pop();
      return task;
    }
    return nullptr;
  }

  void workqueue::addWork(funcType task) {
    tasks.push(task);
  }
}
HPX_REGISTER_COMPONENT_MODULE();

typedef hpx::components::component<workstealing::workqueue> workqueue_type;

HPX_REGISTER_COMPONENT(workqueue_type, workqueue);

HPX_REGISTER_ACTION(workstealing::workqueue::steal_action, workqueue_steal_action);
HPX_REGISTER_ACTION(workstealing::workqueue::addWork_action, workqueue_addWork_action);
