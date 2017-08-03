#ifndef SKELETONS_ENUM_NODEGENERATOR_HPP
#define SKELETONS_ENUM_NODEGENERATOR_HPP

#include <boost/serialization/access.hpp>

namespace skeletons { namespace Enum {

template<typename Space,
         typename Sol>
struct NodeGenerator {
  int numChildren;
  virtual Sol next(const Space & space, const Sol & n) = 0;

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & numChildren;
  }

  // Default implementation. Can be overriden if the problem supports a fast way to compute just the Nth child
  virtual Sol nth(const Space & space, const Sol & node, unsigned n) {
    Sol c;
    for (auto i = 0; i <= n; ++i) {
      c = next(space, node);
    }
    return c;
  };
};

}}

#endif
