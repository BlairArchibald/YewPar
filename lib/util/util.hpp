#ifndef YEWPAR_UTIL_HPP
#define YEWPAR_UTIL_HPP

namespace util {

bool isColocated(hpx::naming::id_type & id) {
  return hpx::get_colocation_id(hpx::launch::sync, id) == hpx::find_here();
}

}

#endif
