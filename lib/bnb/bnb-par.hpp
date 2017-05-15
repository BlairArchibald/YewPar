#ifndef SKELETONS_BNB_PAR_HPP
#define SKELETONS_BNB_PAR_HPP

#include <vector>
#include <hpx/util/tuple.hpp>

#include "registry.hpp"
#include "incumbent.hpp"

namespace skeletons { namespace BnB { namespace Par {

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          typename ChildTask>
void
expand(unsigned spawnDepth,
       const hpx::util::tuple<Sol, Bnd, Cand> & n) {

  auto reg = skeletons::BnB::Components::Registry<Space,Bnd>::gReg;

  Gen gen;
  std::vector< hpx::util::tuple<Sol, Bnd, Cand> > newCands = gen(hpx::find_here(), reg->space_, n);

  std::vector<hpx::future<void> > childFuts;
  if (spawnDepth > 0) {
    childFuts.reserve(newCands.size());
  }

  for (auto const & c : newCands) {
    auto lbnd = reg->localBound_.load();

    /* Prune if required */
    Bound ubound;
    if (ubound(hpx::find_here(), reg->space_, c) < lbnd) {
      continue;
    }

    /* Update incumbent if required */
    if (hpx::util::get<1>(c) > lbnd) {
      typedef typename bounds::Incumbent<Sol, Bnd, Cand>::updateIncumbent_action updateInc;
      reg->localBound_.store(hpx::util::get<1>(c));
      hpx::async<updateInc>(reg->globalIncumbent_, c).get();
    }

    /* Search the child nodes */
    if (spawnDepth > 0) {
      childFuts.push_back(hpx::async<ChildTask>(hpx::find_here(), spawnDepth - 1, c));
    } else {
      expand<Space, Sol, Bnd, Cand, Gen, Bound, ChildTask>(spawnDepth - 1, c);
    }
  }

  if (spawnDepth > 0) {
    hpx::wait_all(childFuts);
  }
}

/* Non-distributed Branch and Bound Search using HPX internal work scheduling policies */
template <typename Space,
        typename Sol,
        typename Bnd,
        typename Cand,
        typename Gen,
        typename Bound,
        typename ChildTask>
hpx::util::tuple<Sol, Bnd, Cand>
search(unsigned spawnDepth,
       const Space & space,
       const hpx::util::tuple<Sol, Bnd, Cand> & root) {

  auto initialBnd = hpx::util::get<1>(root);
  auto incumbent  = hpx::new_<bounds::Incumbent<Sol, Bnd, Cand> >(hpx::find_here()).get();
  skeletons::BnB::Components::initialiseRegistry(space, initialBnd, incumbent);

  typedef typename bounds::Incumbent<Sol, Bnd, Cand>::updateIncumbent_action updateInc;
  hpx::async<updateInc>(incumbent, root).get();

  expand<Space, Sol, Bnd, Cand, Gen, Bound, ChildTask>(spawnDepth, root);

  typedef typename bounds::Incumbent<Sol, Bnd, Cand>::getIncumbent_action getInc;
  return hpx::async<getInc>(incumbent).get();
}

/* FIXME: Not sure this is needed. Can I not just call expand? */
template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          typename ChildTask>
void
searchChildTask(unsigned spawnDepth,
                hpx::util::tuple<Sol, Bnd, Cand> c) {
  expand<Space, Sol, Bnd, Cand, Gen, Bound, ChildTask>(spawnDepth, c);
}

}}}

#endif
