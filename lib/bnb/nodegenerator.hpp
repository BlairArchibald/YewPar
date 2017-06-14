#ifndef SKELETONS_BNB_NODEGENERATOR_HPP
#define SKELETONS_BNB_NODEGENERATOR_HPP

#include <hpx/util/tuple.hpp>
#include <boost/serialization/access.hpp>

// Node Generators wrap a user generation function that when called gives the
// next child node (in left-to-right heuristic order) We explicitly wrap a Gen
// type to allow the user to have a custom constructor

namespace skeletons { namespace BnB  {

template<typename Space,
         typename Sol,
         typename Bnd,
         typename Cand>
struct NodeGenerator {
  int numChildren;
  virtual hpx::util::tuple<Sol, Bnd, Cand> next(const Space & space, const hpx::util::tuple<Sol, Bnd, Cand> & n) = 0;

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & numChildren;
  }

  // Default implementation. Can be overriden if the problem supports a fast way to compute just the Nth child
  virtual hpx::util::tuple<Sol, Bnd, Cand> nth(const Space & space, const hpx::util::tuple<Sol, Bnd, Cand> & node, unsigned n) {
    hpx::util::tuple<Sol, Bnd, Cand> c;
    for (auto i = 0; i <= n; ++i) {
      c = next(space, node);
    }
    return c;
  };
};

}}

#endif
