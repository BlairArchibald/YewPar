#ifndef YEWPAR_UTIL_HPP
#define YEWPAR_UTIL_HPP

#include <vector>

namespace util {

bool isColocated(hpx::naming::id_type & id) {
  return hpx::get_colocation_id(hpx::launch::sync, id) == hpx::find_here();
}

// Find all localities except the one the function is called on
std::vector<hpx::naming::id_type> findOtherLocalities () {
  auto locs = hpx::find_all_localities();
  auto here = hpx::find_here();
  locs.erase(std::remove(locs.begin(), locs.end(), here), locs.end());
  return locs;
}

}

#endif
