#include "DistSetOnceFlag.hpp"

HPX_REGISTER_COMPONENT_MODULE();

typedef hpx::components::component<YewPar::util::DistSetOnceFlag> comp_type;
HPX_REGISTER_COMPONENT(comp_type, dist_set_once_flag);

HPX_REGISTER_ACTION(YewPar::util::DistSetOnceFlag::set_value_action, dist_set_once_flag_set_value_act);

