#ifndef SKELETONS_BNB_DECISION_PAR_HPP
#define SKELETONS_BNB_DECISION_PAR_HPP

#include <vector>
#include <memory>
#include <hpx/util/tuple.hpp>

#include "registry.hpp"
#include "incumbent.hpp"
#include "nodegenerator.hpp"
#include "util/doubleWritePromise.hpp"

namespace skeletons { namespace BnB { namespace Decision { namespace Par {

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          typename ChildTask,
          bool PruneLevel>
void
expand(unsigned spawnDepth,
       const hpx::util::tuple<Sol, Bnd, Cand> & n,
       const hpx::naming::id_type foundProm) {
  constexpr bool const prunelevel = PruneLevel;

  auto reg = skeletons::BnB::Components::Registry<Space,Bnd>::gReg;

  auto newCands = Gen::invoke(0, reg->space_, n);

  std::vector<hpx::future<void> > childFuts;
  if (spawnDepth > 0) {
    childFuts.reserve(newCands.numChildren);
  }

  for (auto i = 0; i < newCands.numChildren; ++i) {
    auto c = newCands.next(reg->space_, n);
    auto lbnd = reg->localBound_.load();

    /* Prune if required */
    auto ubound = Bound::invoke(0, reg->space_, c);
    if (ubound < lbnd) {
      if (prunelevel) {
        break;
      } else {
        continue;
      }
    }

    /* Update incumbent if required */
    if (hpx::util::get<1>(c) == lbnd) {
      typedef typename bounds::Incumbent<Sol, Bnd, Cand>::updateIncumbent_action updateInc;
      hpx::async<updateInc>(reg->globalIncumbent_, c).get();

      /* Found a solution, tell the main thread to kill everything */
      hpx::async<YewPar::util::DoubleWritePromise<bool>::set_value_action>(foundProm, true).get();
    }

    /* Search the child nodes */
    if (spawnDepth > 0) {
      childFuts.push_back(hpx::async<ChildTask>(hpx::find_here(), spawnDepth - 1, c, foundProm));
    } else {
      expand<Space, Sol, Bnd, Cand, Gen, Bound, ChildTask, PruneLevel>(0, c, foundProm);
    }
  }

  if (spawnDepth > 0) {
    hpx::wait_all(childFuts);
  }
}

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          typename ChildTask,
          bool PruneLevel = false>
void
doSearch(unsigned spawnDepth,
         hpx::util::tuple<Sol, Bnd, Cand> c,
         const hpx::naming::id_type foundProm) {
  expand<Space, Sol, Bnd, Cand, Gen, Bound, ChildTask, PruneLevel>(spawnDepth, c, foundProm);

  // Search finished without finding a solution, wake up the main thread to finalise everything
  hpx::async<YewPar::util::DoubleWritePromise<bool>::set_value_action>(foundProm, true).get();
}

/* Non-distributed Branch and Bound Search using HPX internal work scheduling policies */
template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          typename ChildTask,
          bool PruneLevel = false>
hpx::util::tuple<Sol, Bnd, Cand>
search(unsigned spawnDepth,
       const Space & space,
       const hpx::util::tuple<Sol, Bnd, Cand> & root,
       const Bnd & decisionBound) {

  /* Registry stores the bound we are searching for */
  auto incumbent  = hpx::new_<bounds::Incumbent<Sol, Bnd, Cand> >(hpx::find_here()).get();
  skeletons::BnB::Components::initialiseRegistry(space, decisionBound, incumbent);

  typedef typename bounds::Incumbent<Sol, Bnd, Cand>::updateIncumbent_action updateInc;
  hpx::async<updateInc>(incumbent, root).get();

  hpx::promise<int> foundProm;
  auto foundFut    = foundProm.get_future();
  auto foundPromId = foundProm.get_id();
  auto foundId = hpx::new_<YewPar::util::DoubleWritePromise<bool> >(hpx::find_here(), foundPromId).get();

  hpx::apply(hpx::util::bind(&doSearch<Space, Sol, Bnd, Cand, Gen, Bound, ChildTask, PruneLevel>, spawnDepth, root, foundId));

  foundFut.get(); // Block main thread until we get a result

  typedef typename bounds::Incumbent<Sol, Bnd, Cand>::getIncumbent_action getInc;
  return hpx::async<getInc>(incumbent).get();
}

}}}}

#endif
