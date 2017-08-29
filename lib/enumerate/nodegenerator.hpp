#ifndef SKELETONS_ENUM_NODEGENERATOR_HPP
#define SKELETONS_ENUM_NODEGENERATOR_HPP

#include <boost/serialization/access.hpp>

namespace skeletons { namespace Enum {

template<typename Space,
         typename Sol>
struct NodeGenerator {
  int numChildren;
  virtual Sol next() = 0;

  // Default implementation. Can be overriden if the problem supports a fast way to compute just the Nth child
  virtual Sol nth(unsigned n) {
    Sol c;
    for (auto i = 0; i <= n; ++i) {
      c = next();
    }
    return c;
  };
};

}}

#endif
