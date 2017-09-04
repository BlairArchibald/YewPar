#ifndef NODESTACKMANAGER_COMPONENT_HPP
#define NODESTACKMANAGER_COMPONENT_HPP

#include <iterator>                                              // for advance
#include <memory>                                                // for allo...
#include <random>                                                // for defa...
#include <vector>                                                // for vector
#include "hpx/lcos/async.hpp"                                    // for async
#include "hpx/lcos/detail/future_data.hpp"                       // for task...
#include "hpx/lcos/detail/promise_lco.hpp"                       // for prom...
#include "hpx/lcos/future.hpp"                                   // for future
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
#include "util/NodeStack.hpp"
#include <boost/optional.hpp>
#include <boost/serialization/optional.hpp>

// // FIXME: Not sure why hpx doesn't support serialising optional/pair already
namespace hpx { namespace serialization {
    template <typename Archive, typename Sol>
    void serialize(Archive & ar, boost::optional<hpx::util::tuple<Sol, unsigned, hpx::naming::id_type> > & x, const unsigned int version) {
  ar & x;
}}}

namespace workstealing {

template <typename Sol, typename GenType, std::size_t N, typename ChildFunc>
class NodeStackManager : public hpx::components::locking_hook<
  hpx::components::component_base<NodeStackManager<Sol, GenType, N, ChildFunc> > > {
private:
  std::vector<std::shared_ptr<NodeStack<Sol, GenType, N> > > active; // Active node stacks for stealing

  std::vector<hpx::naming::id_type> distributedMgrs;

  boost::optional<hpx::util::tuple<Sol, unsigned, hpx::naming::id_type> > steal() {
    if (active.empty()) {
      return {}; // Nothing to steal
    } else {
      return getLocalWork();
    }
  }

  boost::optional<hpx::util::tuple<Sol, unsigned, hpx::naming::id_type> > tryDistSteal() {
    auto victim = distributedMgrs.begin();

    std::uniform_int_distribution<int> rand(0, distributedMgrs.size() - 1);
    std::default_random_engine randGenerator;
    std::advance(victim, rand(randGenerator));

    return hpx::async<getLocalWork_action>(*victim).get();
  }

public:
  void registerDistributedManagers(std::vector<hpx::naming::id_type> distributedPosMgrs) {
    distributedMgrs = distributedMgrs;
  }
  HPX_DEFINE_COMPONENT_ACTION(NodeStackManager, registerDistributedManagers);

  boost::optional<hpx::util::tuple<Sol, unsigned, hpx::naming::id_type> > getLocalWork() {
    if (active.size() > 0) {
      auto victim = active.begin();

      std::uniform_int_distribution<int> rand(0, active.size() - 1);
      std::default_random_engine randGenerator;
      std::advance(victim, rand(randGenerator));

      return (*victim)->steal();
    }
    return {};
  }
  HPX_DEFINE_COMPONENT_ACTION(NodeStackManager, getLocalWork);

  bool getWork() {
    boost::optional<hpx::util::tuple<Sol, unsigned, hpx::naming::id_type> > p;
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

      auto sol = hpx::util::get<0>(*p);
      auto depth = hpx::util::get<1>(*p);
      auto prom = hpx::util::get<2>(*p);

      // Build the action
      auto stack = std::make_shared<NodeStack<Sol, GenType, N> >(depth);
      active.push_back(stack);

      hpx::threads::executors::current_executor scheduler;
      scheduler.add(hpx::util::bind(ChildFunc::fn_ptr, sol, depth, stack, prom, active.size() - 1, this->get_id()));
    }
    return true;
  }
  HPX_DEFINE_COMPONENT_ACTION(NodeStackManager, getWork);

  void addWork(Sol s, hpx::naming::id_type prom) {
    auto stack = std::make_shared<NodeStack<Sol, GenType, N> >();
    active.push_back(stack);

    hpx::threads::executors::current_executor scheduler;
    scheduler.add(hpx::util::bind(ChildFunc::fn_ptr, s, 1, stack, prom, active.size() - 1, this->get_id()));
  }
  HPX_DEFINE_COMPONENT_ACTION(NodeStackManager, addWork);

  void done(int pos) {
    active.erase(active.begin() + pos);
  }
  HPX_DEFINE_COMPONENT_ACTION(NodeStackManager, done);

};

}

#define REGISTER_NODESTACKMANAGER(sol,genType,N,childFunc)                                                    \
  typedef workstealing::NodeStackManager<sol, genType, N, childFunc > __nodestackmgr_type_;                   \
                                                                                                              \
  HPX_REGISTER_ACTION(__nodestackmgr_type_::registerDistributedManagers_action,                               \
                      BOOST_PP_CAT(__nodestackmgr_registerDistributedManagers_action,                         \
                                   BOOST_PP_CAT(sol, BOOST_PP_CAT(genType, BOOST_PP_CAT(N, childFunc)))));    \
                                                                                                              \
  HPX_REGISTER_ACTION(__nodestackmgr_type_::getWork_action,                                                   \
                      BOOST_PP_CAT(__nodestackmgr_getWork_action,                                             \
                                   BOOST_PP_CAT(sol, BOOST_PP_CAT(genType, BOOST_PP_CAT(N, childFunc)))));    \
                                                                                                              \
  HPX_REGISTER_ACTION(__nodestackmgr_type_::addWork_action,                                                   \
                      BOOST_PP_CAT(__nodestackmgr_addWork_action,                                             \
                                   BOOST_PP_CAT(sol, BOOST_PP_CAT(genType, BOOST_PP_CAT(N, childFunc)))));    \
  HPX_REGISTER_ACTION(__nodestackmgr_type_::done_action,                                                      \
                      BOOST_PP_CAT(__nodestackmgr_done_action,                                                \
                                   BOOST_PP_CAT(sol, BOOST_PP_CAT(genType, BOOST_PP_CAT(N, childFunc)))));    \
  HPX_REGISTER_ACTION(__nodestackmgr_type_::getLocalWork_action,                                              \
                      BOOST_PP_CAT(__nodestackmgr_getLocalWork_action,                                        \
                                   BOOST_PP_CAT(sol, BOOST_PP_CAT(genType, BOOST_PP_CAT(N, childFunc)))));    \
                                                                                                              \
  typedef ::hpx::components::component<__nodestackmgr_type_ >                                                 \
  BOOST_PP_CAT(__nodestackmgr_, BOOST_PP_CAT(sol, BOOST_PP_CAT(genType, BOOST_PP_CAT(N, childFunc))));        \
                                                                                                              \
  HPX_REGISTER_COMPONENT(BOOST_PP_CAT(__nodestackmgr_,                                                        \
                                      BOOST_PP_CAT(sol, BOOST_PP_CAT(genType, BOOST_PP_CAT(N, childFunc))))); \

#endif
