#ifndef YEWPAR_REGISTRY_HPP
#define YEWPAR_REGISTRY_HPP

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iterator>
#include <memory>
#include <vector>

#include <hpx/runtime/actions/basic_action.hpp>
#include <hpx/traits/action_stacksize.hpp>
#include <hpx/lcos/local/mutex.hpp>

#include "skeletons/API.hpp"
#include "Enumerator.hpp"

namespace YewPar {

template <typename Space, typename Node, typename Bound, typename Enumerator>
struct Registry {
  static Registry<Space, Node, Bound, Enumerator>* gReg;

  // General parameters
  Space space;
  Node root;

  Skeletons::API::Params<Bound> params;

  // BNB
  std::atomic<Bound> localBound;
  hpx::naming::id_type globalIncumbent;

  // Decision problems
  std::atomic<bool> stopSearch {false};
  hpx::naming::id_type foundPromiseId;

  // Counting Nodes
  Enumerator acc;
  using MutexT = hpx::lcos::local::mutex;
  MutexT mtx;
  // using countMapT = std::vector<std::atomic<std::uint64_t> >;
  // std::unique_ptr<std::vector<std::atomic<std::uint64_t> > > counts;

  // We construct this object globally at compile time (see below) so this can't
  // happen in the constructor and should instead be called as an action on each
  // locality.
  void initialise(Space space, Node root, Skeletons::API::Params<Bound> params) {
    this->space = space;
    this->root = root;
    this->params = params;
    this->localBound = params.initialBound;
    this->acc = Enumerator();
  }

  // Counting
  void updateEnumerator(Enumerator & e) {
    std::lock_guard<MutexT> l(mtx);
    acc.combine(e.get());
  }

  using ResT = typename Enumerator::ResT;
  ResT getEnumeratorVal() {
    return acc.get();
  }

  // BNB
  template <typename Cmp>
  void updateRegistryBound(Bound bnd) {
    while (true) {
      auto curBound = localBound.load();
      Cmp cmp;
      if (!cmp(bnd, curBound)) {
        break;
      }

      if (localBound.compare_exchange_weak(curBound, bnd)) {
        break;
      }
    }
  }

  void setStopSearchFlag() {
    stopSearch.store(true);
  }

};

template<typename Space, typename Node, typename Bound, typename Enumerator>
Registry<Space, Node, Bound, Enumerator>* Registry<Space, Node, Bound, Enumerator>::gReg = new Registry<Space, Node, Bound, Enumerator>;

// Easy calling
template <typename Space, typename Node, typename Bound, typename Enumerator>
void initialiseRegistry(Space space, Node root, YewPar::Skeletons::API::Params<Bound> params) {
  Registry<Space, Node, Bound, Enumerator>::gReg->initialise(space, root, params);
}
template <typename Space, typename Node, typename Bound, typename Enumerator>
struct InitRegistryAct : hpx::actions::make_direct_action<
  decltype(&initialiseRegistry<Space, Node, Bound, Enumerator>), &initialiseRegistry<Space, Node, Bound, Enumerator>, InitRegistryAct<Space, Node, Bound, Enumerator> >::type {};

template <typename Space, typename Node, typename Bound, typename Enumerator>
typename Enumerator::ResT getEnumeratorVal() {
  return Registry<Space, Node, Bound, Enumerator>::gReg->getEnumeratorVal();
}
template <typename Space, typename Node, typename Bound, typename Enumerator>
struct GetEnumeratorValAct : hpx::actions::make_direct_action<
  decltype(&getEnumeratorVal<Space, Node, Bound, Enumerator>), &getEnumeratorVal<Space, Node, Bound, Enumerator>, GetEnumeratorValAct<Space, Node, Bound, Enumerator> >::type {};

template <typename Space, typename Node, typename Bound, typename Enumerator>
void setStopSearchFlag() {
  Registry<Space, Node, Bound, Enumerator>::gReg->setStopSearchFlag();
}
template <typename Space, typename Node, typename Bound, typename Enumerator>
struct SetStopFlagAct : hpx::actions::make_direct_action<
  decltype(&setStopSearchFlag<Space, Node, Bound, Enumerator>), &setStopSearchFlag<Space, Node, Bound, Enumerator>, SetStopFlagAct<Space, Node, Bound, Enumerator> >::type {};

template <typename Space, typename Node, typename Bound, typename Enumerator, typename Cmp>
void updateRegistryBound(Bound bnd) {
  auto reg = Registry<Space, Node, Bound, Enumerator>::gReg;
  (*reg).template updateRegistryBound<Cmp>(bnd);
}
template <typename Space, typename Node, typename Bound, typename Enumerator, typename Cmp>
struct UpdateRegistryBoundAct : hpx::actions::make_direct_action<
  decltype(&updateRegistryBound<Space, Node, Bound, Enumerator, Cmp>), &updateRegistryBound<Space, Node, Bound, Enumerator, Cmp>, UpdateRegistryBoundAct<Space, Node, Bound, Enumerator, Cmp> >::type {};

template <typename Space, typename Node, typename Bound, typename Enumerator>
void updateGlobalIncumbent(hpx::naming::id_type inc) {
  Registry<Space, Node, Bound, Enumerator>::gReg->globalIncumbent = inc;
}
template <typename Space, typename Node, typename Bound, typename Enumerator>
struct UpdateGlobalIncumbentAct : hpx::actions::make_direct_action<
  decltype(&updateGlobalIncumbent<Space, Node, Bound, Enumerator>), &updateGlobalIncumbent<Space, Node, Bound, Enumerator>, UpdateGlobalIncumbentAct<Space, Node, Bound, Enumerator> >::type {};

template <typename Space, typename Node, typename Bound, typename Enumerator>
void setFoundPromiseId(hpx::naming::id_type id) {
  Registry<Space, Node, Bound, Enumerator>::gReg->foundPromiseId = id;
}
template <typename Space, typename Node, typename Bound, typename Enumerator>
struct SetFoundPromiseIdAct : hpx::actions::make_direct_action<
  decltype(&setFoundPromiseId<Space, Node, Bound, Enumerator>), &setFoundPromiseId<Space, Node, Bound, Enumerator>, SetFoundPromiseIdAct<Space, Node, Bound, Enumerator> >::type {};

} // YewPar

namespace hpx { namespace traits {
template <typename Space, typename Node, typename Bound, typename Enumerator>
struct action_stacksize<YewPar::InitRegistryAct<Space, Node, Bound, Enumerator> > {
  enum { value = threads::thread_stacksize_huge };
};

template <typename Space, typename Node, typename Bound, typename Enumerator>
struct action_stacksize<YewPar::SetFoundPromiseIdAct<Space, Node, Bound, Enumerator> > {
  enum { value = threads::thread_stacksize_huge };
};

template <typename Space, typename Node, typename Bound, typename Enumerator>
struct action_stacksize<YewPar::UpdateGlobalIncumbentAct<Space, Node, Bound, Enumerator> > {
  enum { value = threads::thread_stacksize_huge };
};

template <typename Space, typename Node, typename Bound, typename Enumerator, typename Cmp>
struct action_stacksize<YewPar::UpdateRegistryBoundAct<Space, Node, Bound, Enumerator, Cmp> > {
  enum { value = threads::thread_stacksize_huge };
};

template <typename Space, typename Node, typename Bound, typename Enumerator>
struct action_stacksize<YewPar::SetStopFlagAct<Space, Node, Bound, Enumerator> > {
  enum { value = threads::thread_stacksize_huge };
};

template <typename Space, typename Node, typename Bound, typename Enumerator>
struct action_stacksize<YewPar::GetEnumeratorValAct<Space, Node, Bound, Enumerator> > {
  enum { value = threads::thread_stacksize_huge };
};

}}

#endif
