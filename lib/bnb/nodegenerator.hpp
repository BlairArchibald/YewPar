#ifndef SKELETONS_BNB_NODEGENERATOR_HPP
#define SKELETONS_BNB_NODEGENERATOR_HPP

#include <hpx/util/tuple.hpp>

// Node Generators wrap a user generation function that when called gives the
// next child node (in left-to-right heuristic order) We explicitly wrap a Gen
// type to allow the user to have a custom constructor

namespace skeletons { namespace BnB  {

template<typename Sol,
         typename Bnd,
         typename Cand>
struct NodeGenerator {
  int numChildren;
  virtual hpx::util::tuple<Sol, Bnd, Cand> next() = 0;
};

}}

#endif
