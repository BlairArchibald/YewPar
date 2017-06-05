#include "workqueue.hpp"

namespace workstealing
{
  using funcType = hpx::util::function<void(hpx::naming::id_type)>;
  funcType workqueue::steal() {
    funcType task;
    if (!tasks.pop_right(task)) {
      return nullptr;
    }
    return task;
  }

  funcType workqueue::getLocal() {
    funcType task;
    if (!tasks.pop_left(task)) {
      return nullptr;
    }
    return task;
  }

  void workqueue::addWork(funcType task) {
    tasks.push_left(task);
  }
}
HPX_REGISTER_COMPONENT_MODULE();

typedef hpx::components::component<workstealing::workqueue> workqueue_type;

HPX_REGISTER_COMPONENT(workqueue_type, workqueue);

HPX_REGISTER_ACTION(workstealing::workqueue::getLocal_action, workqueue_getLocal_action);
HPX_REGISTER_ACTION(workstealing::workqueue::steal_action, workqueue_steal_action);
HPX_REGISTER_ACTION(workstealing::workqueue::addWork_action, workqueue_addWork_action);
