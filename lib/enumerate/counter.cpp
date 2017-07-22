#include "counter.hpp"

HPX_REGISTER_COMPONENT_MODULE();

typedef hpx::components::component<Enum::Counter> counter_type;

HPX_REGISTER_COMPONENT(counter_type, EnumCounter);

HPX_REGISTER_ACTION(Enum::Counter::add_action, enum_add_action);
HPX_REGISTER_ACTION(Enum::Counter::getCount_action, enum_getCount_action);
