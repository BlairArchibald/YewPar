#ifndef SKELETONS_BNB_SEQ_HPP
#define SKELETONS_BNB_SEQ_HPP

#include <hpx/util/tuple.hpp>

namespace skeletons { namespace BnB { namespace Seq {

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          bool PruneLevel = false>
struct BranchAndBoundOpt {
  static void expand(const Space & space,
                     hpx::util::tuple<Sol, Bnd, Cand> & incumbent,
                     const hpx::util::tuple<Sol, Bnd, Cand> & n) {
    constexpr bool const prunelevel = PruneLevel;

    auto newCands = Gen::invoke(space, n);

    for (auto i = 0; i < newCands.numChildren; ++i) {
      auto c = newCands.next();

      /* Prune if required */
      if (Bound::invoke(space, c) <= hpx::util::get<1>(incumbent)) {
        // TODO: check this is optimised away
        if (prunelevel) {
          break;
        } else {
          continue;
        }
      }

      /* Update incumbent if required */
      if (hpx::util::get<1>(c) > hpx::util::get<1>(incumbent)) {
        incumbent = c;
      }

      /* Search the child nodes */
      expand(space, incumbent, c);
    }
  }

  static hpx::util::tuple<Sol, Bnd, Cand> search(const Space & space,
                                                 const hpx::util::tuple<Sol, Bnd, Cand> & root) {
    auto incumbent = root;
    expand(space, incumbent, root);
    return incumbent;
  }

};

}}}

#endif
