#ifndef UTIL_LAZY_NODEGENERATOR_HPP
#define UTIL_LAZY_NODEGENERATOR_HPP

namespace YewPar {

#include <hpx/util/tuple.hpp>

template <typename NodeType>
struct NodeGenerator {
  unsigned numChildren;

  NodeGenerator() : numChildren(0) {};
  NodeGenerator(unsigned children) : numChildren(children) {};

  // When called, return the next child element
  // Pre condition: numChildren < number of next Calls
  virtual NodeType next() = 0;

  // Quickly skip to the nth child if possible Useful for recompute based
  // skeletons where we send a path in the tree rather than a particular node
  virtual NodeType nth(unsigned n) {
    NodeType c;
    for (auto i = 0; i <= n; ++i) {
      c = next();
    }
    return c;
  };
};

// BnB Nodes are represented as a (CurrentSol, LowerBound, RemainingSpace) tuple
template<typename Sol, typename Bnd, typename Cand>
struct BnBNodeGenerator : NodeGenerator<hpx::util::tuple<Sol, Bnd, Cand> > {};

}

#endif
