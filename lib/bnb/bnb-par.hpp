#ifndef SKELETONS_BNB_PAR_HPP
#define SKELETONS_BNB_PAR_HPP

#include <vector>
#include <hpx/util/tuple.hpp>

#include "registry.hpp"
#include "incumbent.hpp"

namespace skeletons { namespace BnB { namespace Par {

/* B&B using HPX internal queues only */
template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          bool PruneLevel = false>
struct BranchAndBoundOpt {

  struct ChildTask : hpx::actions::make_action<
    decltype(&BranchAndBoundOpt<Space, Sol, Bnd, Cand, Gen, Bound, PruneLevel>::expand),
    &BranchAndBoundOpt<Space, Sol, Bnd, Cand, Gen, Bound, PruneLevel>::expand,
    ChildTask>::type {};

  static void expand(unsigned spawnDepth,
                     const hpx::util::tuple<Sol, Bnd, Cand> & n) {
    constexpr bool const prunelevel = PruneLevel;

    auto reg = skeletons::BnB::Components::Registry<Space, Sol, Bnd, Cand>::gReg;

    auto newCands = Gen::invoke(reg->space_, n);

    std::vector<hpx::future<void> > childFuts;
    if (spawnDepth > 0) {
      childFuts.reserve(newCands.numChildren);
    }

    for (auto i = 0; i < newCands.numChildren; ++i) {
      auto c = newCands.next();
      auto lbnd = reg->localBound_.load();

      /* Prune if required */
      auto ubound = Bound::invoke(reg->space_, c);
      if (ubound <= lbnd) {
        if (prunelevel) {
          break;
        } else {
          continue;
        }
      }

      /* Update incumbent if required */
      if (hpx::util::get<1>(c) > lbnd) {
        skeletons::BnB::Components::updateRegistryBound<Space, Sol, Bnd, Cand>(hpx::util::get<1>(c));

        typedef typename bounds::Incumbent<Sol, Bnd, Cand>::updateIncumbent_action updateInc;
        hpx::async<updateInc>(reg->globalIncumbent_, c).get();
      }

      /* Search the child nodes */
      if (spawnDepth > 0) {
        childFuts.push_back(hpx::async<ChildTask>(hpx::find_here(), spawnDepth - 1, c));
      } else {
        expand(0, c);
      }
    }

    if (spawnDepth > 0) {
      hpx::wait_all(childFuts);
    }
  }

  static hpx::util::tuple<Sol, Bnd, Cand> search(unsigned spawnDepth,
                                                 const Space & space,
                                                 const hpx::util::tuple<Sol, Bnd, Cand> & root) {

    auto initialBnd = hpx::util::get<1>(root);
    auto incumbent  = hpx::new_<bounds::Incumbent<Sol, Bnd, Cand> >(hpx::find_here()).get();
    skeletons::BnB::Components::initialiseRegistry(space, initialBnd, incumbent, root);

    typedef typename bounds::Incumbent<Sol, Bnd, Cand>::updateIncumbent_action updateInc;
    hpx::async<updateInc>(incumbent, root).get();

    expand(spawnDepth, root);

    typedef typename bounds::Incumbent<Sol, Bnd, Cand>::getIncumbent_action getInc;
    return hpx::async<getInc>(incumbent).get();
  }
};

}}}

#endif
