#ifndef SKELETONS_BNB_INDEXED_HPP
#define SKELETONS_BNB_INDEXED_HPP

#include <vector>
#include <hpx/util/tuple.hpp>
#include <hpx/lcos/promise.hpp>
#include <hpx/lcos/broadcast.hpp>
#include <hpx/runtime/components/new.hpp>

#include "registry.hpp"
#include "incumbent.hpp"
#include "util/positionIndex.hpp"

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
struct BranchAndBoundOpt {

  static void expand(positionIndex & pos,
                     const hpx::util::tuple<Sol, Bnd, Cand> & n) {
    constexpr bool const prunelevel = PruneLevel;

    auto reg = skeletons::BnB::Components::Registry<Space, Sol, Bnd, Cand>::gReg;

    auto newCands = Gen::invoke(reg->space_, n);
    pos.setNumChildren(newCands.numChildren);

    auto i = 0;
    int nextPos;
    while ((nextPos = pos.getNextPosition()) >= 0) {
      auto c = newCands.next();

      if (nextPos != i) {
        for (auto j = 0; j < nextPos - i; ++j) {
          c = newCands.next();
        }
        i += nextPos - i;
      }

      auto lbnd = reg->localBound_.load();

      /* Prune if required */
      auto ubound = Bound::invoke(reg->space_, c);
      if (ubound <= lbnd) {
        if (prunelevel) {
          pos.pruneLevel();
          break; // numberChildren = 0?
        } else {
          ++i;
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
      expand(pos, c);
      pos.postExpand();

      ++i;
    }
  }

  static hpx::util::tuple<Sol, Bnd, Cand> search(const Space & space,
                                                 const hpx::util::tuple<Sol, Bnd, Cand> & root) {

    // Initialise the registries on all localities
    auto bnd = hpx::util::get<1>(root);
    auto inc = hpx::new_<bounds::Incumbent<Sol, Bnd, Cand> >(hpx::find_here()).get();
    hpx::wait_all(hpx::lcos::broadcast<initRegistry_act>(hpx::find_all_localities(), space, bnd, inc, root));

    // Initialise the global incumbent
    typedef typename bounds::Incumbent<Sol, Bnd, Cand>::updateIncumbent_action updateInc;
    hpx::async<updateInc>(inc, root).get();

    // Initialise positionManagers on each node to handle steals
    auto posMgrs = hpx::lcos::broadcast<workstealing::indexed::initPosMgr_act<ChildTask> >(hpx::find_all_localities()).get();

    // TODO: Track this on each node when we init the managers
    hpx::naming::id_type localPosMgr;
    for (auto it = posMgrs.begin(); it != posMgrs.end(); ++it) {
      if (hpx::get_colocation_id(*it).get() == hpx::find_here()) {
        localPosMgr = *it;
        break;
      }
    }

    // Start work stealing schedulers on all localities
    hpx::async<startScheduler_indexed_action>(hpx::find_here(), posMgrs).get();

    std::vector<unsigned> path;
    path.reserve(30);
    path.push_back(0);
    hpx::promise<void> prom;
    auto f = prom.get_future();
    auto pid = prom.get_id();
    hpx::async<workstealing::indexed::posManager::addWork_action>(localPosMgr, path, pid);

    // Wait completion of the main task
    f.get();

    // Stop all work stealing schedulers
    hpx::wait_all(hpx::lcos::broadcast<stopScheduler_indexed_action>(hpx::find_all_localities()));

    // Read the result form the global incumbent
    typedef typename bounds::Incumbent<Sol, Bnd, Cand>::getIncumbent_action getInc;
    return hpx::async<getInc>(inc).get();
  }

  static hpx::util::tuple<Sol, Bnd, Cand> getStartingNode(std::vector<unsigned> path) {
    auto reg = skeletons::BnB::Components::Registry<Space, Sol, Bnd, Cand>::gReg;
    auto node =  reg->root_;


    // Paths have a leading 0 (representing root, we don't need this).
    path.erase(path.begin());

    if (path.empty()) {
      return node;
    }

    for (auto const & p : path) {
      auto newCands = Gen::invoke(reg->space_, node);
      node = newCands.nth(p);
    }

    return node;
  }

  static void searchChildTask(const std::shared_ptr<positionIndex> posIdx,
                              const hpx::naming::id_type p,
                              const int idx,
                              const hpx::naming::id_type posMgr) {
    auto c = getStartingNode(posIdx->getPath());
    expand(*posIdx, c);
    hpx::async<workstealing::indexed::posManager::done_action>(posMgr, idx).get();

    // Don't fully finish until we determine all children are also finished - Termination detection
    workstealing::indexed::tasks_required_sem.signal();
    posIdx->waitFutures();
    hpx::async<hpx::lcos::base_lco_with_value<void>::set_value_action>(p, true).get();
  }

  struct ChildTask : hpx::actions::make_action<
    decltype(&BranchAndBoundOpt<Space, Sol, Bnd, Cand, Gen, Bound, PruneLevel>::searchChildTask),
    &BranchAndBoundOpt<Space, Sol, Bnd, Cand, Gen, Bound, PruneLevel>::searchChildTask,
    ChildTask>::type {};

};

}}}

#endif
