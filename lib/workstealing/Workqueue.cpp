#include "Workqueue.hpp"
#include "hpx/runtime/components/component_factory.hpp"        // for compon...
#include "hpx/runtime/components/component_factory_base.hpp"   // for compon...
#include "hpx/runtime/components/component_registry.hpp"       // for compon...
#include "hpx/runtime/components/component_registry_base.hpp"  // for compon...
#include "hpx/runtime/components/component_type.hpp"           // for compon...
#include "hpx/runtime/components/server/component.hpp"         // for component
#include "hpx/runtime/components/static_factory_data.hpp"      // for init_r...
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
