#ifndef SKELETONS_BNB_SEQ_HPP
#define SKELETONS_BNB_SEQ_HPP

#include <hpx/util/tuple.hpp>
#include "nodegenerator.hpp"

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
                     const hpx::util::tuple<Sol, Bnd, Cand> & n,
                     const Gen && gen,
                     const Bound && ubound) {
    constexpr bool const prunelevel = PruneLevel;

    auto newCands = gen(space, n);

    for (auto i = 0; i < newCands.numChildren; ++i) {
      auto c = newCands.next(space, n);

      /* Prune if required */
      if (ubound(space, c) <= hpx::util::get<1>(incumbent)) {
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
      expand(space, incumbent, c, gen, ubound);
    }
  }

  static hpx::util::tuple<Sol, Bnd, Cand> search(const Space & space,
                                                 const hpx::util::tuple<Sol, Bnd, Cand> & root,
                                                 const Gen && gen,
                                                 const Bound && ubound) {
    auto incumbent = root;
    expand(space, incumbent, root, gen, ubound);
    return incumbent;
  }

};

}}}

#endif
