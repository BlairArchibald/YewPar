#ifndef WORKQUEUE_COMPONENT_HPP
#define WORKQUEUE_COMPONENT_HPP

#include <hpx/util/lockfree/deque.hpp>                           // for deque
#include "hpx/runtime/actions/basic_action.hpp"                  // for HPX_...
#include "hpx/runtime/actions/component_action.hpp"              // for HPX_...
#include "hpx/runtime/actions/transfer_action.hpp"               // for tran...
#include "hpx/runtime/actions/transfer_continuation_action.hpp"  // for tran...
#include "hpx/runtime/components/server/component_base.hpp"      // for comp...
#include "hpx/runtime/naming/name.hpp"                           // for intr...
#include "hpx/runtime/serialization/serialize.hpp"               // for oper...
#include "hpx/traits/needs_automatic_registration.hpp"           // for need...
#include "hpx/util/function.hpp"                                 // for func...
namespace hpx { namespace naming { struct id_type; } }

namespace workstealing
{
  class Workqueue : public hpx::components::component_base<Workqueue> {
    private:
      using funcType = hpx::util::function<void(hpx::naming::id_type)>;
      boost::lockfree::deque<funcType> tasks; // From HPX

    public:
      funcType getLocal();
      HPX_DEFINE_COMPONENT_ACTION(Workqueue, getLocal);
      funcType steal();
      HPX_DEFINE_COMPONENT_ACTION(Workqueue, steal);
      void addWork(funcType task);
      HPX_DEFINE_COMPONENT_ACTION(Workqueue, addWork);
    };
}

HPX_REGISTER_ACTION_DECLARATION(workstealing::Workqueue::getLocal_action, Workqueue_getLocal_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::Workqueue::steal_action, Workqueue_steal_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::Workqueue::addWork_action, Workqueue_addWork_action);

#endif
