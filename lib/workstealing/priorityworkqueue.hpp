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
namespace hpx { namespace naming { struct id_type; } }

namespace workstealing
{
  namespace detail {
    struct priorityworkqueueCompare {
      using funcType  = hpx::util::function<void(hpx::naming::id_type)>;
      using queueType = hpx::util::tuple<int, funcType>;

      bool operator()(queueType a, queueType b) {
        auto i1 = hpx::util::get<0>(a);
        auto i2 = hpx::util::get<0>(b);
        return i1 < i2;
      }
    };
  }

  class priorityworkqueue : public hpx::components::locking_hook<
    hpx::components::component_base<priorityworkqueue>
    >
    {
    private:
      using funcType  = hpx::util::function<void(hpx::naming::id_type)>;
      using queueType = hpx::util::tuple<int, funcType>;

      std::priority_queue<queueType, std::vector<queueType>, detail::priorityworkqueueCompare> tasks;

    public:
      funcType steal();
      HPX_DEFINE_COMPONENT_ACTION(priorityworkqueue, steal);
      void addWork(int priority, funcType task);
      HPX_DEFINE_COMPONENT_ACTION(priorityworkqueue, addWork);
      bool workRemaining();
      HPX_DEFINE_COMPONENT_ACTION(priorityworkqueue, workRemaining);
    };
}

HPX_REGISTER_ACTION_DECLARATION(workstealing::priorityworkqueue::steal_action, workqueue_prio_steal_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::priorityworkqueue::addWork_action, workqueue_prio_addWork_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::priorityworkqueue::workRemaining_action, workqueue_prio_workRemaining_action);

#endif
