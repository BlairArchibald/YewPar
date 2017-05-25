#include "workqueue.hpp"

namespace workstealing
{
  using funcType = hpx::util::function<void(hpx::naming::id_type)>;
  funcType workqueue::steal() {
    if (!tasks.empty()) {
      auto task = tasks.back();
      tasks.pop_back();
      return task;
    }
    return nullptr;
  }

  funcType workqueue::getLocal() {
    if (!tasks.empty()) {
      auto task = tasks.front();
      tasks.pop_front();
      return task;
    }
    return nullptr;
  }

  void workqueue::addWork(funcType task) {
    tasks.push_front(task);
  }
}
HPX_REGISTER_COMPONENT_MODULE();

typedef hpx::components::component<workstealing::workqueue> workqueue_type;

HPX_REGISTER_COMPONENT(workqueue_type, workqueue);

HPX_REGISTER_ACTION(workstealing::workqueue::getLocal_action, workqueue_getLocal_action);
HPX_REGISTER_ACTION(workstealing::workqueue::steal_action, workqueue_steal_action);
HPX_REGISTER_ACTION(workstealing::workqueue::addWork_action, workqueue_addWork_action);
