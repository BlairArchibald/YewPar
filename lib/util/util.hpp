#ifndef YEWPAR_UTIL_HPP
#define YEWPAR_UTIL_HPP

#include <vector>
#include <hpx/modules/runtime_distributed.hpp>

namespace YewPar { namespace util {

bool isColocated(hpx::id_type & id);

// Find all localities except the one the function is called on
std::vector<hpx::id_type> findOtherLocalities ();

}}

#endif
