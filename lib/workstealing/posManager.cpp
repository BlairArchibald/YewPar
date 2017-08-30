#include "posManager.hpp"

#include <hpx/runtime/components/component_factory.hpp>

HPX_REGISTER_COMPONENT_MODULE();

typedef hpx::components::component<workstealing::indexed::posManager> posMgr_type;
HPX_REGISTER_COMPONENT(posMgr_type, posManager);

HPX_REGISTER_ACTION(workstealing::indexed::posManager::registerDistributedManagers_action, posManager_registerDistributedManagers_action);
HPX_REGISTER_ACTION(workstealing::indexed::posManager::getWork_action, posManager_getWork_action);
HPX_REGISTER_ACTION(workstealing::indexed::posManager::addWork_action, posManager_addWork_action);
HPX_REGISTER_ACTION(workstealing::indexed::posManager::done_action, posManager_done_action);
