#include "util.hpp"

#include <hpx/hpx.hpp>
#include <hpx/async_colocated/get_colocation_id.hpp>

namespace YewPar { namespace util {

bool isColocated(hpx::id_type & id) {
  return hpx::get_colocation_id(hpx::launch::sync, id) == hpx::find_here();
}

// Find all localities except the one the function is called on
std::vector<hpx::id_type> findOtherLocalities () {
  auto locs = hpx::find_all_localities();
  auto here = hpx::find_here();
  locs.erase(std::remove(locs.begin(), locs.end(), here), locs.end());
  return locs;
}

}}
