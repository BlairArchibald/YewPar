#ifndef PRIORITY_WORKQUEUE_COMPONENT_HPP
#define PRIORITY_WORKQUEUE_COMPONENT_HPP

#include <hpx/util/tuple.hpp>                                    // for get
#include <queue>                                                 // for prio...
#include <vector>                                                // for vector
#include "hpx/runtime/actions/basic_action.hpp"                  // for HPX_...
#include "hpx/runtime/actions/component_action.hpp"              // for HPX_...
#include "hpx/runtime/actions/transfer_action.hpp"               // for tran...
#include "hpx/runtime/actions/transfer_continuation_action.hpp"  // for tran...
#include "hpx/runtime/components/server/component_base.hpp"      // for comp...
#include "hpx/runtime/components/server/locking_hook.hpp"        // for lock...
#include "hpx/runtime/naming/name.hpp"                           // for intr...
#include "hpx/runtime/serialization/serialize.hpp"               // for oper...
#include "hpx/runtime/threads/thread_data_fwd.hpp"               // for get_...
#include "hpx/traits/is_action.hpp"                              // for is_a...
#include "hpx/traits/needs_automatic_registration.hpp"           // for need...
#include "hpx/util/function.hpp"                                 // for func...
#include "hpx/util/lockfree/deque.hpp"                           // for deque

namespace hpx { namespace naming { struct id_type; } }

namespace workstealing {
  class priorityworkqueue : public hpx::components::component_base<priorityworkqueue> {
    private:
      using funcType  = hpx::util::function<void(hpx::naming::id_type)>;
      boost::lockfree::deque<funcType> tasks;

    public:
      funcType steal();
      HPX_DEFINE_COMPONENT_ACTION(priorityworkqueue, steal);

      // Add the entire vector of tasks to the workqueue
      // Used in the Ordered skeleton to add the initial work
      // Up to the caller to put the tasks in the correct order.
      // *NOT* threadsafe with steal, so tasks should be added ahead of time.
      void addWork(std::vector<funcType> tasks);
      HPX_DEFINE_COMPONENT_ACTION(priorityworkqueue, addWork);
    };
}

HPX_REGISTER_ACTION_DECLARATION(workstealing::priorityworkqueue::steal_action, workqueue_prio_steal_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::priorityworkqueue::addWork_action, workqueue_prio_addWork_action);

#endif
