#ifndef SKELETONS_BNB_SEQ_HPP
#define SKELETONS_BNB_SEQ_HPP

#include <vector>

#include <hpx/util/tuple.hpp>
#include "nodegenerator.hpp"

namespace skeletons { namespace BnB { namespace Seq {

static unsigned long numExpands = 0;

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound>
void
expand(const Space & space,
      hpx::util::tuple<Sol, Bnd, Cand> & incumbent,
      const hpx::util::tuple<Sol, Bnd, Cand> & n,
      const Gen && gen,
      const Bound && ubound) {

  auto newCands = gen(space, n);
  numExpands++;

  for (auto i = 0; i < newCands.numChildren; ++i) {
    auto c = newCands.next(space, n);

    /* Prune if required */
    if (ubound(space, c) <= hpx::util::get<1>(incumbent)) {
      //continue;
      break; // Prune Level Optimisation
    }

    /* Update incumbent if required */
    if (hpx::util::get<1>(c) > hpx::util::get<1>(incumbent)) {
      incumbent = c;
    }

    /* Search the child nodes */
    expand(space, incumbent, c, gen, ubound);
  }
}

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound>
hpx::util::tuple<Sol, Bnd, Cand>
search(const Space & space,
       const hpx::util::tuple<Sol, Bnd, Cand> & root,
       const Gen && gen,
       const Bound && ubound) {
  auto incumbent = root;
  expand(space, incumbent, root, gen, ubound);
  return incumbent;
}

}}}

#endif
