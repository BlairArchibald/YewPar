#ifndef WORKQUEUE_COMPONENT_HPP
#define WORKQUEUE_COMPONENT_HPP

#include <hpx/hpx.hpp>
#include <hpx/include/components.hpp>

#include <hpx/util/lockfree/deque.hpp>

namespace workstealing
{
  class workqueue : public hpx::components::component_base<workqueue> {
    private:
      using funcType = hpx::util::function<void(hpx::naming::id_type)>;
      boost::lockfree::deque<funcType> tasks; // From HPX

    public:
      funcType getLocal();
      HPX_DEFINE_COMPONENT_ACTION(workqueue, getLocal);
      funcType steal();
      HPX_DEFINE_COMPONENT_ACTION(workqueue, steal);
      void addWork(funcType task);
      HPX_DEFINE_COMPONENT_ACTION(workqueue, addWork);
    };
}

HPX_REGISTER_ACTION_DECLARATION(workstealing::workqueue::getLocal_action, workqueue_getLocal_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::workqueue::steal_action, workqueue_steal_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::workqueue::addWork_action, workqueue_addWork_action);

#endif
