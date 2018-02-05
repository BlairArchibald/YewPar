#ifndef YEWPAR_UTIL_HPP
#define YEWPAR_UTIL_HPP

#include <vector>

#include <hpx/runtime/find_here.hpp>

namespace YewPar { namespace util {

bool isColocated(hpx::naming::id_type & id);

// Find all localities except the one the function is called on
std::vector<hpx::naming::id_type> findOtherLocalities ();

}}

#endif
