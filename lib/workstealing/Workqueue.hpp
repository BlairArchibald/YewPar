#ifndef WORKQUEUE_COMPONENT_HPP
#define WORKQUEUE_COMPONENT_HPP

#include <hpx/include/components.hpp>
#include <hpx/concurrency/deque.hpp>
#include <hpx/functional/function.hpp>

namespace workstealing
{
  class Workqueue : public hpx::components::component_base<Workqueue> {
    public:
      using funcType = hpx::distributed::function<void(hpx::id_type)>;

      funcType getLocal();
      HPX_DEFINE_COMPONENT_ACTION(Workqueue, getLocal);
      funcType steal();
      HPX_DEFINE_COMPONENT_ACTION(Workqueue, steal);
      void addWork(funcType task);
      HPX_DEFINE_COMPONENT_ACTION(Workqueue, addWork);

    private:
      hpx::lockfree::deque<funcType> tasks; // From HPX
    };
}

HPX_REGISTER_ACTION_DECLARATION(workstealing::Workqueue::getLocal_action, Workqueue_getLocal_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::Workqueue::steal_action, Workqueue_steal_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::Workqueue::addWork_action, Workqueue_addWork_action);

#endif
