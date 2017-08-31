#ifndef POSMANAGER_COMPONENT_HPP
#define POSMANAGER_COMPONENT_HPP

#include <iterator>                                              // for advance
#include <memory>                                                // for allo...
#include <random>                                                // for defa...
#include <utility>                                               // for pair
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
#include "util/positionIndex.hpp"                                // for posi...

namespace workstealing { namespace indexed {

class posManager : public hpx::components::locking_hook<
  hpx::components::component_base<posManager > > {
private:
  std::vector<std::shared_ptr<positionIndex> > active; // Active position indices for stealing
  std::vector<hpx::naming::id_type> distributedMgrs;

  using funcType = hpx::util::function<void(const std::shared_ptr<positionIndex>,
                                            const hpx::naming::id_type,
                                            const int,
                                            const hpx::naming::id_type)>;
  std::unique_ptr<funcType> fn;

public:
  posManager() {};
  posManager(std::unique_ptr<funcType> f) : fn(std::move(f)) {};

  void registerDistributedManagers(std::vector<hpx::naming::id_type> distributedPosMgrs);
  HPX_DEFINE_COMPONENT_ACTION(posManager, registerDistributedManagers);

  std::pair<std::vector<unsigned>, hpx::naming::id_type> getLocalWork();

  struct getLocalWork_action : hpx::actions::make_action<
    decltype(&workstealing::indexed::posManager::getLocalWork),
    &workstealing::indexed::posManager::getLocalWork,
    getLocalWork_action> ::type {};

  std::pair<std::vector<unsigned>, hpx::naming::id_type> steal();

  std::pair<std::vector<unsigned>, hpx::naming::id_type> tryDistSteal();

  bool getWork();
  HPX_DEFINE_COMPONENT_ACTION(posManager, getWork);

  void addWork(std::vector<unsigned> path, hpx::naming::id_type prom);
  HPX_DEFINE_COMPONENT_ACTION(posManager, addWork);

  void done(int pos);
  HPX_DEFINE_COMPONENT_ACTION(posManager, done);
};

// Helpers for initialising positionManagers with the given task to run on a steal
template <typename ChildTask>
hpx::naming::id_type initPosMgr() {
  using funcType = hpx::util::function<void(const std::shared_ptr<positionIndex>,
                                            const hpx::naming::id_type,
                                            const int,
                                            const hpx::naming::id_type)>;

  auto fn = std::make_unique<funcType>(hpx::util::bind(ChildTask(),
                                                       hpx::find_here(),
                                                       hpx::util::placeholders::_1,
                                                       hpx::util::placeholders::_2,
                                                       hpx::util::placeholders::_3,
                                                       hpx::util::placeholders::_4));

  return hpx::components::local_new<posManager>(std::move(fn)).get();
}

template <typename ChildTask>
struct initPosMgr_act : hpx::actions::make_action<
  decltype(&initPosMgr<ChildTask>),
  &initPosMgr<ChildTask>,
  initPosMgr_act<ChildTask> > ::type {};
}}

HPX_REGISTER_ACTION_DECLARATION(workstealing::indexed::posManager::registerDistributedManagers_action, posManager_registerDistributedManagers_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::indexed::posManager::getWork_action, posManager_getWork_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::indexed::posManager::addWork_action, posManager_addWork_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::indexed::posManager::done_action, posManager_done_action);



#endif
