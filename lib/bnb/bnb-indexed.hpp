#ifndef SKELETONS_BNB_INDEXED_HPP
#define SKELETONS_BNB_INDEXED_HPP

#include <vector>
#include <hpx/util/tuple.hpp>
#include <hpx/lcos/broadcast.hpp>
#include <hpx/runtime/components/new.hpp>

#include "registry.hpp"
#include "incumbent.hpp"
#include "positionIndex.hpp"

#include "workstealing/indexedScheduler.hpp"
#include "workstealing/posManager.hpp"

namespace skeletons { namespace BnB { namespace Indexed {

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          bool PruneLevel = false>
void searchChildTask(std::shared_ptr<positionIndex> posIdx, hpx::naming::id_type p);

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          bool PruneLevel>
void
expand(positionIndex & pos, const hpx::util::tuple<Sol, Bnd, Cand> & n) {
  constexpr bool const prunelevel = PruneLevel;

  auto reg = skeletons::BnB::Components::Registry<Space, Sol, Bnd, Cand>::gReg;

  auto newCands = Gen::invoke(0, reg->space_, n);
  pos.setNumChildren(newCands.numChildren);

  auto i = 0;
  int nextPos;
  while ((nextPos = pos.getNextPosition()) > 0) {
    auto c = newCands.next(reg->space_, n);

    // TODO: Can probably write this neater. Also might want to "skip" via generator functionality
    // Make up the distance if the nextPos isn't i
    if (nextPos != i) {
      for (auto j = 0; j < nextPos - i; ++j) {
        auto c = newCands.next(reg->space_, n);
      }
      i += nextPos - i;
    }

    auto lbnd = reg->localBound_.load();

    /* Prune if required */
    auto ubound = Bound::invoke(0, reg->space_, c);
    if (ubound <= lbnd) {
      if (prunelevel) {
        pos.pruneLevel();
        break; // numberChildren = 0?
      } else {
        continue;
      }
    }

    /* Update incumbent if required */
    if (hpx::util::get<1>(c) > lbnd) {
      skeletons::BnB::Components::updateRegistryBound<Space, Sol, Bnd, Cand>(hpx::util::get<1>(c));
      hpx::lcos::broadcast<updateRegistryBound_act>(hpx::find_all_localities(), hpx::util::get<1>(c));

      typedef typename bounds::Incumbent<Sol, Bnd, Cand>::updateIncumbent_action act;
      hpx::async<act>(reg->globalIncumbent_, c).get();
    }

    pos.preExpand(i);
    expand<Space, Sol, Bnd, Cand, Gen, Bound, PruneLevel>(pos, c);
    pos.postExpand();

    ++i;
  }

  pos.waitFutures(); // Wait for any futures to fill before backtracking
}

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          bool PruneLevel = false>
hpx::util::tuple<Sol, Bnd, Cand>
search(const Space & space, const hpx::util::tuple<Sol, Bnd, Cand> & root) {

  // Initialise the registries on all localities
  auto bnd = hpx::util::get<1>(root);
  auto inc = hpx::new_<bounds::Incumbent<Sol, Bnd, Cand> >(hpx::find_here()).get();
  hpx::wait_all(hpx::lcos::broadcast<initRegistry_act>(hpx::find_all_localities(), space, bnd, inc, root));

  // Initialise the global incumbent
  typedef typename bounds::Incumbent<Sol, Bnd, Cand>::updateIncumbent_action updateInc;
  hpx::async<updateInc>(inc, root).get();

  // Workstealing structures
  auto childFn = std::make_unique<hpx::util::function<void(std::shared_ptr<positionIndex>, hpx::naming::id_type)> >(searchChildTask<Space, Sol, Bnd, Cand, Gen, Bound, PruneLevel>);
  auto posMgr = hpx::components::local_new<workstealing::indexed::posManager>(std::move(childFn)).get();

  // Start work stealing schedulers on all localities
  hpx::async<startScheduler_indexed_action>(hpx::find_here(), posMgr).get();

  positionIndex pos;
  expand<Space, Sol, Bnd, Cand, Gen, Bound, PruneLevel>(pos, root);

  // Stop all work stealing schedulers
  hpx::wait_all(hpx::lcos::broadcast<stopScheduler_indexed_action>(hpx::find_all_localities()));

  // Read the result form the global incumbent
  typedef typename bounds::Incumbent<Sol, Bnd, Cand>::getIncumbent_action getInc;
  return hpx::async<getInc>(inc).get();
}

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound>
hpx::util::tuple<Sol, Bnd, Cand>
getStartingNode(std::vector<unsigned> path) {
  auto reg = skeletons::BnB::Components::Registry<Space, Sol, Bnd, Cand>::gReg;
  auto node =  reg->root_;

  // Paths have a leading 0 (representing root, we don't need this).
  if (!path.empty()) {
    path.erase(path.begin());
  }

  for (auto const & p : path) {
    auto newCands = Gen::invoke(0, reg->space_, node);
    node = newCands.nth(reg->space_, node, p);
  }

  return node;
}

template <typename Space,
          typename Sol,
          typename Bnd,
          typename Cand,
          typename Gen,
          typename Bound,
          bool PruneLevel = false>
void
searchChildTask(std::shared_ptr<positionIndex> posIdx,
                hpx::naming::id_type p) {
  auto c = getStartingNode<Space, Sol, Bnd, Cand, Gen, Bound>(posIdx->getPath());
  expand<Space, Sol, Bnd, Cand, Gen, Bound, PruneLevel>(*posIdx, c);
  workstealing::indexed::tasks_required_sem.signal();
  hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(p, true).get();
}

}}}

#endif
