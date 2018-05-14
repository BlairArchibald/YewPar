#ifndef YEWPAR_DIST_WRITE_ONCE_FLAG_HPP
#define YEWPAR_DIST_WRITE_ONCE_FLAG_HPP

#include <hpx/hpx.hpp>

// A distributed boolean flag that allows basic synchronisation between two distributed threads. If the flag is set return false else set it and return true.
namespace YewPar { namespace util {

class DistSetOnceFlag : public hpx::components::locking_hook<
  hpx::components::component_base<DistSetOnceFlag> > {

private:
  bool written;

public:
  DistSetOnceFlag() : written(false) {};

  bool set_value() {
    if (!written) {
      written = true;
      return true;
    }
    return false;
  }
  HPX_DEFINE_COMPONENT_ACTION(DistSetOnceFlag, set_value);
};

}}

HPX_REGISTER_ACTION_DECLARATION(YewPar::util::DistSetOnceFlag::set_value_action, dist_set_once_flag_set_value_act);

#endif
