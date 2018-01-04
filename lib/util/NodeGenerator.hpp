#ifndef UTIL_LAZY_NODEGENERATOR_HPP
#define UTIL_LAZY_NODEGENERATOR_HPP

namespace YewPar {

#include <hpx/util/tuple.hpp>

struct Empty {};

template <typename NodeType, typename Space = Empty>
struct NodeGenerator {
  using Nodetype  = NodeType;
  using Spacetype = Space;

  unsigned numChildren;

  // When called, return the next child element
  // Pre condition: numChildren < number of next Calls
  virtual NodeType next() = 0;

  // Quickly skip to the nth child if possible Useful for recompute based
  // skeletons where we send a path in the tree rather than a particular node
  NodeType nth(unsigned n) {
    NodeType c;
    for (auto i = 0; i <= n; ++i) {
      c = next();
    }
    return c;
  };
};

}

#endif
