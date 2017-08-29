#ifndef SKELETONS_BNB_DECISION_SEQ_HPP
#define SKELETONS_BNB_DECISION_SEQ_HPP

#include <vector>
#include <hpx/util/tuple.hpp>

namespace skeletons { namespace BnB { namespace Seq {

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          bool PruneLevel = false>
struct BranchAndBoundSat {

  static bool expand(const Space & space,
                     hpx::util::tuple<Sol, Bnd, Cand> & incumbent,
                     const hpx::util::tuple<Sol, Bnd, Cand> & n,
                     const Bnd & expectedBound) {
    constexpr bool const prunelevel = PruneLevel;

    auto newCands = Gen::invoke(space, n);

    for (auto i = 0; i < newCands.numChildren; ++i) {
      auto c = newCands.next();

      /* Prune if required */
      if (Bound::invoke(space, c) < expectedBound) {
        if (prunelevel) {
          break;
        } else {
          continue;
        }
      }

      /* Update incumbent if required */
      if (hpx::util::get<1>(c) == expectedBound) {
        incumbent = c;
        return true;
      }

      /* Search the child nodes */
      auto found = expand(space, incumbent, c, expectedBound);
      if (found) {
        return true; // Propagate the fact we have found the solution up the stack.
      }

    }
    return false;
  }

  static hpx::util::tuple<Sol, Bnd, Cand> search(const Space & space,
                                                 const hpx::util::tuple<Sol, Bnd, Cand> & root,
                                                 const Bnd & expectedBound) {
    auto incumbent = root;
    expand(space, incumbent, root, expectedBound);
    return incumbent;
  }

};

}}}

#endif
