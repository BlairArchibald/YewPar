#ifndef SKELETONS_BNB_SEQ_HPP
#define SKELETONS_BNB_SEQ_HPP

#include <hpx/util/tuple.hpp>

namespace skeletons { namespace BnB { namespace Seq {

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound>
void
expand(const Space & space,
      hpx::util::tuple<Sol, Bnd, Cand> & incumbent,
      const std::util::tuple<Sol, Bnd, Cand> & n,
      const Gen && gen,
      const Bound && ubound) {
  auto newCands = gen(space, n);
  for (auto const & c : newCands) {

    /* Prune if required */
    if (ubound(space, c) < std::get<1>(incumbent)) {
      continue;
    }

    /* Update incumbent if required */
    if (std::get<1>(c) > std::get<1>(incumbent)) {
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
