#include "counter.hpp"

HPX_REGISTER_COMPONENT_MODULE();

typedef hpx::components::component<skeletons::Enum::Components::Counter> counter_type;

HPX_REGISTER_COMPONENT(counter_type, EnumCounter);

HPX_REGISTER_ACTION(skeletons::Enum::Components::Counter::add_action, enum_add_action);
HPX_REGISTER_ACTION(skeletons::Enum::Components::Counter::getCountMap_action, enum_getCountMap_action);
