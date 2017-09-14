#ifndef NODESTACKMANAGER_COMPONENT_HPP
#define NODESTACKMANAGER_COMPONENT_HPP

#include <iterator>                                              // for advance
#include <memory>                                                // for allo...
#include <random>                                                // for defa...
#include <vector>                                                // for vector
#include <utility>                                                // for vector
#include <queue>
#include <unordered_map>
#include "hpx/lcos/async.hpp"                                    // for async
#include "hpx/lcos/detail/future_data.hpp"                       // for task...
#include "hpx/lcos/detail/promise_lco.hpp"                       // for prom...
#include "hpx/lcos/future.hpp"                                   // for future
#include "hpx/lcos/local/channel.hpp"                            // for future
#include "hpx/runtime/actions/basic_action.hpp"                  // for make...
#include "hpx/runtime/actions/component_action.hpp"              // for HPX_...
#include "hpx/runtime/actions/transfer_action.hpp"               // for tran...
#include "hpx/runtime/actions/transfer_continuation_action.hpp"  // for tran...
#include "hpx/runtime/components/new.hpp"                        // for loca...
#include "hpx/runtime/components/server/component_base.hpp"      // for comp...
#include "hpx/runtime/components/server/locking_hook.hpp"        // for lock...
#include "hpx/runtime/find_here.hpp"                             // for find...
#include "hpx/runtime/naming/id_type.hpp"                        // for id_type
#include "hpx/runtime/naming/name.hpp"                           // for intr...
#include "hpx/runtime/serialization/binary_filter.hpp"           // for bina...
#include "hpx/runtime/serialization/serialize.hpp"               // for oper...
#include "hpx/runtime/serialization/shared_ptr.hpp"
#include "hpx/runtime/threads/executors/current_executor.hpp"    // for curr...
#include "hpx/runtime/threads/thread_data_fwd.hpp"               // for get_...
#include "hpx/traits/is_action.hpp"                              // for is_a...
#include "hpx/traits/needs_automatic_registration.hpp"           // for need...
#include "hpx/util/bind.hpp"                                     // for bound
#include "hpx/util/function.hpp"                                 // for func...
#include "hpx/util/unused.hpp"                                   // for unus...
#include "hpx/util/tuple.hpp"
#include <boost/optional.hpp>
#include <boost/serialization/optional.hpp>

// // FIXME: Not sure why hpx doesn't support serialising optional/pair already
namespace hpx { namespace serialization {
    template <typename Archive, typename Sol>
    void serialize(Archive & ar, boost::optional<hpx::util::tuple<Sol, int, hpx::naming::id_type> > & x, const unsigned int version) {
  ar & x;
}}}

namespace workstealing {

template <typename Sol, typename ChildFunc>
class NodeStackManager : public hpx::components::locking_hook<
  hpx::components::component_base<NodeStackManager<Sol, ChildFunc> > > {
private:
  using Response_t = boost::optional<hpx::util::tuple<Sol, int, hpx::naming::id_type> >;
  using SharedState_t = std::pair<std::atomic<bool>, hpx::lcos::local::one_element_channel<Response_t> >;

  std::queue<unsigned> activeIds;
  std::unordered_map<unsigned, std::shared_ptr<SharedState_t> > active; // Active flags/channels stacks for stealing
  std::unordered_map<unsigned, std::shared_ptr<SharedState_t> > inactive; // Stacks already being stolen from
  std::vector<hpx::naming::id_type> distributedMgrs;

  Response_t steal() {
    if (active.empty()) {
      return {}; // Nothing to steal
    } else {
      return getLocalWork();
    }
  }

  Response_t tryDistSteal() {
    auto victim = distributedMgrs.begin();

    std::uniform_int_distribution<int> rand(0, distributedMgrs.size() - 1);
    std::default_random_engine randGenerator;
    std::advance(victim, rand(randGenerator));

    return hpx::async<getLocalWork_action>(*victim).get();
  }

public:

  NodeStackManager() {
    auto numThreads = hpx::get_os_thread_count();
    for (auto i = 0; i < numThreads; ++i) {
      activeIds.push(i);
    }
  }

  void registerDistributedManagers(std::vector<hpx::naming::id_type> distributedPosMgrs) {
    distributedMgrs = distributedMgrs;
  }
  HPX_DEFINE_COMPONENT_ACTION(NodeStackManager, registerDistributedManagers);

  Response_t getLocalWork() {
    if (!active.empty()) {
      auto victim = active.begin();

      std::uniform_int_distribution<int> rand(0, active.size() - 1);
      std::default_random_engine randGenerator;
      std::advance(victim, rand(randGenerator));

      auto pos = victim->first;
      auto stealReqPtr = victim->second;

      // We remove the victim from active while we steal, so that if we suspend
      // no other thread gets in the way of our steal
      active.erase(pos);
      inactive[pos] = stealReqPtr;

      // Signal the thread that we need work from it and wait for some (or Nothing)
      std::get<0>(*stealReqPtr).store(true);
      auto resF = std::get<1>(*stealReqPtr).get();
      auto res = resF.get();

      if (res && hpx::util::get<1>(*res) == -1) {
        return {};
      }

      active[pos] = stealReqPtr;
      inactive.erase(pos);
      return res;
    }
    return {};
  }
  HPX_DEFINE_COMPONENT_ACTION(NodeStackManager, getLocalWork);

  bool getWork() {
    Response_t p;
    if (active.empty()) {
      // Steal distributed if there is no local work
      if (distributedMgrs.size() > 0) {
        p = tryDistSteal();
      } else {
        return false;
      }
    } else {
      p = getLocalWork();
    }

    if (!p) {
      // Couldn't schedule anything locally
      return false;
    } else {
      Sol sol   = hpx::util::get<0>(*p);
      unsigned depth = hpx::util::get<1>(*p);
      hpx::naming::id_type prom  = hpx::util::get<2>(*p);

      // Build the action
      auto shared_state = std::make_shared<SharedState_t>();
      auto nextId = activeIds.front();
      activeIds.pop();
      active[nextId] = shared_state;

      hpx::threads::executors::current_executor scheduler;
      scheduler.add(hpx::util::bind(ChildFunc::fn_ptr(), sol, depth, shared_state, prom, nextId, this->get_id()));
    }
    return true;
  }
  HPX_DEFINE_COMPONENT_ACTION(NodeStackManager, getWork);

  void addWork(Sol s, hpx::naming::id_type prom) {
    auto shared_state = std::make_shared<SharedState_t>();
    auto nextId = activeIds.front();
    activeIds.pop();
    active[nextId] = shared_state;

    hpx::threads::executors::current_executor scheduler;
    scheduler.add(hpx::util::bind(ChildFunc::fn_ptr(), s, 1, shared_state, prom, nextId, this->get_id()));
  }
  HPX_DEFINE_COMPONENT_ACTION(NodeStackManager, addWork);

  void done(unsigned pos) {
    if (active.find(pos) != active.end()) {
      active.erase(pos);
    } else {
      // A steal must be in progress on this idx so cancel it before finishing
      // Canceled steals (-1 flag) are already removed from active
      auto & state = inactive[pos];
      std::get<1>(*state).set(hpx::util::make_tuple(Sol(), -1, hpx::find_here()));
      inactive.erase(pos);
    }
    activeIds.push(pos);
  }
  HPX_DEFINE_COMPONENT_ACTION(NodeStackManager, done);

};

}

#define REGISTER_NODESTACKMANAGER(sol,childFunc)                                      \
  typedef workstealing::NodeStackManager<sol, childFunc > __nodestackmgr_type_;       \
                                                                                      \
  HPX_REGISTER_ACTION(__nodestackmgr_type_::registerDistributedManagers_action,       \
                      BOOST_PP_CAT(__nodestackmgr_registerDistributedManagers_action, \
                                   BOOST_PP_CAT(sol, childFunc)));                    \
                                                                                      \
  HPX_REGISTER_ACTION(__nodestackmgr_type_::getWork_action,                           \
                      BOOST_PP_CAT(__nodestackmgr_getWork_action,                     \
                                   BOOST_PP_CAT(sol, childFunc)));                    \
                                                                                      \
  HPX_REGISTER_ACTION(__nodestackmgr_type_::addWork_action,                           \
                      BOOST_PP_CAT(__nodestackmgr_addWork_action,                     \
  BOOST_PP_CAT(sol, childFunc)));                                                     \
                                                                                      \
  HPX_REGISTER_ACTION(__nodestackmgr_type_::done_action,                              \
                      BOOST_PP_CAT(__nodestackmgr_done_action,                        \
  BOOST_PP_CAT(sol, childFunc)));                                                     \
                                                                                      \
  HPX_REGISTER_ACTION(__nodestackmgr_type_::getLocalWork_action,                      \
                      BOOST_PP_CAT(__nodestackmgr_getLocalWork_action,                \
  BOOST_PP_CAT(sol, childFunc)));                                                     \
                                                                                      \
  typedef ::hpx::components::component<__nodestackmgr_type_ >                         \
     BOOST_PP_CAT(__nodestackmgr_, BOOST_PP_CAT(sol, childFunc));                     \
                                                                                      \
  HPX_REGISTER_COMPONENT(BOOST_PP_CAT(__nodestackmgr_,                                \
                                      BOOST_PP_CAT(sol, childFunc)));                 \

#endif
