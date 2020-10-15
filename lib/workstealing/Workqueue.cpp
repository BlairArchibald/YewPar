#include "Workqueue.hpp"

namespace workstealing
{
  using funcType = hpx::util::function<void(hpx::naming::id_type)>;
  funcType Workqueue::steal() {
    funcType task;
    if (!tasks.pop_right(task)) {
      return nullptr;
    }
    return task;
  }

  funcType Workqueue::getLocal() {
    funcType task;
    if (!tasks.pop_left(task)) {
      return nullptr;
    }
    return task;
  }

  void Workqueue::addWork(funcType task) {
    tasks.push_left(task);
  }
}
HPX_REGISTER_COMPONENT_MODULE();

typedef hpx::components::component<workstealing::Workqueue> Workqueue_type;

HPX_REGISTER_COMPONENT(Workqueue_type, Workqueue);

HPX_REGISTER_ACTION(workstealing::Workqueue::getLocal_action, Workqueue_getLocal_action);
HPX_REGISTER_ACTION(workstealing::Workqueue::steal_action, Workqueue_steal_action);
HPX_REGISTER_ACTION(workstealing::Workqueue::addWork_action, Workqueue_addWork_action);
