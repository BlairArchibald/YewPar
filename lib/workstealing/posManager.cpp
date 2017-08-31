#include "posManager.hpp"
#include "hpx/runtime/components/component_factory.hpp"        // for compon...
#include "hpx/runtime/components/component_factory_base.hpp"   // for compon...
#include "hpx/runtime/components/component_registry.hpp"       // for compon...
#include "hpx/runtime/components/component_registry_base.hpp"  // for compon...
#include "hpx/runtime/components/component_type.hpp"           // for compon...
#include "hpx/runtime/components/server/component.hpp"         // for component
#include "hpx/runtime/components/static_factory_data.hpp"      // for init_r...
#include "hpx/runtime/components/unique_component_name.hpp"    // for unique...
#include "hpx/traits/component_type_database.hpp"              // for compon...
#include "hpx/util/plugin/concrete_factory.hpp"                // for concre...
#include "hpx/util/plugin/export_plugin.hpp"                   // for actname
namespace hpx { namespace util { namespace plugin { template <class BasePlugin> struct abstract_factory; } } }
namespace workstealing { namespace indexed {

void posManager::registerDistributedManagers(std::vector<hpx::naming::id_type> distributedPosMgrs) {
  distributedMgrs = distributedPosMgrs;
}

std::pair<std::vector<unsigned>, hpx::naming::id_type> posManager::getLocalWork() {
  std::pair<std::vector<unsigned>, hpx::naming::id_type> empty;
  if (active.size() > 0) {
    auto victim = active.begin();

    std::uniform_int_distribution<int> rand(0, active.size() - 1);
    std::default_random_engine randGenerator;
    std::advance(victim, rand(randGenerator));

    return (*victim)->steal();
  }
  return empty;
}

std::pair<std::vector<unsigned>, hpx::naming::id_type> posManager::steal() {
  hpx::naming::id_type na;
  std::pair<std::vector<unsigned>, hpx::naming::id_type> empty;
  if (active.empty()) {
    return empty; // Nothing to steal
  } else {
    return getLocalWork();
  }
}

std::pair<std::vector<unsigned>, hpx::naming::id_type> posManager::tryDistSteal() {
  auto victim = distributedMgrs.begin();

  std::uniform_int_distribution<int> rand(0, distributedMgrs.size() - 1);
  std::default_random_engine randGenerator;
  std::advance(victim, rand(randGenerator));

  return hpx::async<getLocalWork_action>(*victim).get();
}

bool posManager::getWork() {
  std::pair<std::vector<unsigned>, hpx::naming::id_type> p;
  if (active.empty()) {
    // Steal distributed if there is no local work
    if (distributedMgrs.size() > 0) {
      p = tryDistSteal();
    } else {
      return false;
    }
  } else {
    p = getLocalWork();
  }

  auto path = std::get<0>(p);
  auto prom = std::get<1>(p);

  if (path.empty()) {
    // Couldn't schedule anything locally
    return false;
  } else {
    // Build the action
    auto posIdx = std::make_shared<positionIndex>(path);
    active.push_back(posIdx);

    hpx::threads::executors::current_executor scheduler;
    scheduler.add(hpx::util::bind(*fn, posIdx, prom, active.size() - 1, this->get_id()));

    return true;
  }
}
  HPX_DEFINE_COMPONENT_ACTION(posManager, getWork);

void posManager::addWork(std::vector<unsigned> path, hpx::naming::id_type prom) {
  auto posIdx = std::make_shared<positionIndex>(path);
  active.push_back(posIdx);
  hpx::threads::executors::current_executor scheduler;
  scheduler.add(hpx::util::bind(*fn, posIdx, prom, active.size() - 1, this->get_id()));
}

void posManager::done(int pos) {
  active.erase(active.begin() + pos);
}

}}

HPX_REGISTER_COMPONENT_MODULE();

typedef hpx::components::component<workstealing::indexed::posManager> posMgr_type;
HPX_REGISTER_COMPONENT(posMgr_type, posManager);

HPX_REGISTER_ACTION(workstealing::indexed::posManager::registerDistributedManagers_action, posManager_registerDistributedManagers_action);
HPX_REGISTER_ACTION(workstealing::indexed::posManager::getWork_action, posManager_getWork_action);
HPX_REGISTER_ACTION(workstealing::indexed::posManager::addWork_action, posManager_addWork_action);
HPX_REGISTER_ACTION(workstealing::indexed::posManager::done_action, posManager_done_action);
