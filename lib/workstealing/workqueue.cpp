#include "workqueue.hpp"
#include "hpx/runtime/components/component_factory.hpp"        // for compon...
#include "hpx/runtime/components/component_factory_base.hpp"   // for compon...
#include "hpx/runtime/components/component_registry.hpp"       // for compon...
#include "hpx/runtime/components/component_registry_base.hpp"  // for compon...
#include "hpx/runtime/components/component_type.hpp"           // for compon...
#include "hpx/runtime/components/server/component.hpp"         // for component
#include "hpx/runtime/components/static_factory_data.hpp"      // for init_r...
#include "hpx/runtime/components/unique_component_name.hpp"    // for unique...
#include "hpx/traits/component_type_database.hpp"              // for compon...
#include "hpx/util/lockfree/deque.hpp"                         // for deque
#include "hpx/util/plugin/concrete_factory.hpp"                // for concre...
#include "hpx/util/plugin/export_plugin.hpp"                   // for actname
#include "hpx/lcos/base_lco_with_value.hpp"
namespace hpx { namespace naming { struct id_type; } }
namespace hpx { namespace util { namespace plugin { template <class BasePlugin> struct abstract_factory; } } }

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
