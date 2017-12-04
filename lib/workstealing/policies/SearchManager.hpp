#ifndef YEWPAR_SEARCHMANAGER_COMPONENT_HPP
#define YEWPAR_SEARCHMANAGER_COMPONENT_HPP

#include <iterator>                                              // for advance
#include <memory>                                                // for allo...
#include <random>                                                // for defa...
#include <vector>                                                // for vector
#include <utility>                                               // for vector
#include <queue>
#include <atomic>
#include <unordered_map>
#include "hpx/performance_counters/manage_counter_type.hpp"    // for instal...
#include "hpx/lcos/async.hpp"                                    // for async
#include "hpx/lcos/detail/future_data.hpp"                       // for task...
#include "hpx/lcos/detail/promise_lco.hpp"                       // for prom...
#include "hpx/lcos/future.hpp"                                   // for future
#include "hpx/lcos/local/channel.hpp"                            // for future
#include "hpx/lcos/local/mutex.hpp"
#include "hpx/runtime/actions/plain_action.hpp"
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
#include "hpx/runtime/threads/executors/default_executor.hpp"
#include "hpx/runtime/threads/thread_data_fwd.hpp"               // for get_...
#include "hpx/traits/is_action.hpp"                              // for is_a...
#include "hpx/traits/needs_automatic_registration.hpp"           // for need...
#include "hpx/util/bind.hpp"                                     // for bound
#include "hpx/util/function.hpp"                                 // for func...
#include "hpx/util/unused.hpp"                                   // for unus...
#include "hpx/util/tuple.hpp"
#include "hpx/lcos/broadcast.hpp"
#include "hpx/runtime/get_ptr.hpp"
#include "hpx/util/lockfree/deque.hpp"

#include "Policy.hpp"
#include "workstealing/Scheduler.hpp"
#include "util/util.hpp"

namespace Workstealing { namespace Policies {

namespace SearchManagerPerf {
// Performance Counters
std::atomic<std::uint64_t> perf_localSteals(0);
std::atomic<std::uint64_t> perf_distributedSteals(0);
std::atomic<std::uint64_t> perf_failedLocalSteals(0);
std::atomic<std::uint64_t> perf_failedDistributedSteals(0);

std::vector<std::pair<hpx::naming::id_type, bool> > distributedStealsList;

void registerPerformanceCounters();

void printDistributedStealsList();
HPX_DEFINE_PLAIN_ACTION(printDistributedStealsList, printDistributedStealsList_act);

}}}

namespace Workstealing { namespace Policies {

// The SearchManager component allows steals to happen directly within a
// searching thread. The SearchManager maintains a list of active threads and
// uses this to perform steals when work is requested from the scheduler. Thread
// response is generic allowing responses to be enumeration Nodes, B&B Nodes or
// even paths for recompute based skeletons
template <typename SearchInfo, typename FuncToCall>
class SearchManager : public hpx::components::component_base<SearchManager<SearchInfo, FuncToCall> >,
                      public Policy
{
 private:
  // Lock to protect the component
  // We don't use locking_hook here since we also need to protect the local only "registerThread" function
  using MutexT = hpx::lcos::local::mutex;
  MutexT mtx;

  // Information returned on a steal from a thread
  using Task_t = hpx::util::tuple<SearchInfo, int, hpx::naming::id_type>;

  // We return an empty vector here to signal no tasks
  using Response_t = std::vector<Task_t>;

  // Information shared between a thread and the manager. We set the atomic on a steal and then use the channel to await a response
  //using SharedState_t = std::pair<std::atomic<bool>, hpx::lcos::local::one_element_channel<Response_t> >;
  using SharedState_t = std::tuple<std::atomic<bool>, hpx::lcos::local::one_element_channel<Response_t>, bool>;

  // (internal) Id's of currently running threads, for managing the active map
  std::queue<unsigned> activeIds;

  // Active thread shared states
  std::unordered_map<unsigned, std::shared_ptr<SharedState_t> > active;

  // Shared states of threads currently being stolen from
  std::unordered_map<unsigned, std::shared_ptr<SharedState_t> > inactive;

  // Pointers to SearchManagers on other localities
  std::vector<hpx::naming::id_type> distributedSearchManagers;

  // random number generator
  std::mt19937 randGenerator;

  // Are we currently doing a distributed steal?
  bool isStealingDistributed = false;

  // Task Buffer for chunking
  boost::lockfree::deque<Task_t> taskBuffer;

  // Should we steal all tasks at a level when we perform a steal?
  bool stealAll = false;

  // Try to steal from a thread on another (random) locality
  Response_t tryDistributedSteal(std::unique_lock<MutexT> & l) {
    // We only allow one distributed steal to happen at a time (to make sure we
    // don't overload the communication)
    if (isStealingDistributed) {
      return {};
    }

    isStealingDistributed = true;

    auto victim = distributedSearchManagers.begin();

    std::uniform_int_distribution<> rand(0, distributedSearchManagers.size() - 1);
    std::advance(victim, rand(randGenerator));

    l.unlock();
    auto res = hpx::async<getDistributedWork_action>(*victim).get();
    l.lock();

    isStealingDistributed = false;

    if (!res.empty()) {
      SearchManagerPerf::distributedStealsList.push_back(std::make_pair(*victim, true));
    } else {
      SearchManagerPerf::distributedStealsList.push_back(std::make_pair(*victim, false));
    }

    return res;
  }

 public:

  SearchManager() : SearchManager(false) {}

  SearchManager(bool stealAll) : stealAll(stealAll){
    for (auto i = 0; i < hpx::get_os_thread_count(); ++i) {
      activeIds.push(i);
    }
    std::random_device rd;
    randGenerator.seed(rd());
  }

  // Notify this search manager of the globalId's of potential steal victims
  void registerDistributedManagers(std::vector<hpx::naming::id_type> distributedSearchMgrs) {
    std::lock_guard<MutexT> l(mtx);
    distributedSearchManagers = distributedSearchMgrs;
    distributedSearchManagers.erase(
        std::remove_if(distributedSearchManagers.begin(), distributedSearchManagers.end(), util::isColocated),
        distributedSearchManagers.end());
  }
  HPX_DEFINE_COMPONENT_ACTION(SearchManager, registerDistributedManagers);

  // Try to get work from a (random) thread running on this locality and wrap it
  // back up for serializing over the network
  Response_t getDistributedWork() {
    std::unique_lock<MutexT> l(mtx);
    return getLocalWork(l);
  }
  HPX_DEFINE_COMPONENT_ACTION(SearchManager, getDistributedWork);

  // Try to get work from a (random) thread running on this locality
  Response_t getLocalWork(std::unique_lock<MutexT> & l) {
    if (active.empty()) {
      return {};
    }
    auto victim = active.begin();

    std::uniform_int_distribution<> rand(0, active.size() - 1);
    std::advance(victim, rand(randGenerator));

    auto pos         = victim->first;
    auto stealReqPtr = victim->second;

    // We remove the victim from active while we steal, so that if we suspend
    // no other thread gets in the way of our steal
    active.erase(pos);
    inactive[pos] = stealReqPtr;

    // Signal the thread that we need work from it and wait for some (or Nothing)
    std::get<0>(*stealReqPtr).store(true);
    auto resF = std::get<1>(*stealReqPtr).get();
    l.unlock();
    auto res = resF.get();
    l.lock();

    // -1 depth signals that the thread we tried to steal from has finished it's search
    if (!res.empty() && hpx::util::get<1>(res[0]) == -1) {
      return {};
    }

    // Allow this thread to be stolen from again
    active[pos] = stealReqPtr;
    inactive.erase(pos);
    return res;
  }

  // Called by the scheduler to ask the searchManager to add more work
  // TODO: Tidy up the logic here
  hpx::util::function<void(), false> getWork() override {
    std::unique_lock<MutexT> l(mtx);

    // Return from task buffer first if anything exists
    Task_t task;
    if (taskBuffer.pop_right(task)) {
      SearchInfo searchInfo      = hpx::util::get<0>(task);
      int depth                  = hpx::util::get<1>(task);
      hpx::naming::id_type prom  = hpx::util::get<2>(task);

      // Build the action
      auto shared_state = std::make_shared<SharedState_t>();
      std::get<2>(*shared_state) = stealAll;

      auto nextId = activeIds.front();
      activeIds.pop();
      active[nextId] = shared_state;

      return hpx::util::bind(FuncToCall::fn_ptr(), searchInfo, depth, shared_state, prom, nextId, this->get_id());
    }

    Response_t maybeStolen;
    if (active.empty()) {
      // No local threads running, steal distributed
      if (!distributedSearchManagers.empty()) {
        maybeStolen = tryDistributedSteal(l);
        if (!maybeStolen.empty()) {
          SearchManagerPerf::perf_distributedSteals++;
        } else {
          SearchManagerPerf::perf_failedDistributedSteals++;
          return nullptr;
        }
      } else {
        SearchManagerPerf::perf_failedLocalSteals++;
        return nullptr;
      }
    } else {
      maybeStolen = getLocalWork(l);
      if (!maybeStolen.empty()) {
        SearchManagerPerf::perf_localSteals++;
      } else {
        SearchManagerPerf::perf_failedLocalSteals++;
        return nullptr;
      }
    }

    if (!maybeStolen.empty()) {
      // Take off the first task and queue up anything else that was returned
      auto first = maybeStolen[0];
      SearchInfo searchInfo      = hpx::util::get<0>(first);
      int depth                  = hpx::util::get<1>(first);
      hpx::naming::id_type prom  = hpx::util::get<2>(first);

      auto itr = maybeStolen.begin();
      ++itr;
      for (itr; itr != maybeStolen.end(); ++itr) {
        taskBuffer.push_left(std::move(*itr));
      }

      // Build the action
      auto shared_state = std::make_shared<SharedState_t>();
      std::get<2>(*shared_state) = stealAll;

      auto nextId = activeIds.front();
      activeIds.pop();
      active[nextId] = shared_state;

      return hpx::util::bind(FuncToCall::fn_ptr(), searchInfo, depth, shared_state, prom, nextId, this->get_id());
    }
  }

  // Action to allow work to be pushed eagerly to this searchManager
  void addWork(SearchInfo info, int depth, hpx::naming::id_type prom) {
    std::lock_guard<MutexT> l(mtx);

    auto shared_state = std::make_shared<SharedState_t>();
    std::get<2>(*shared_state) = stealAll;

    auto nextId = activeIds.front();
    activeIds.pop();
    active[nextId] = shared_state;

    // Launch in a new Scheduler
    hpx::threads::executors::default_executor exe(hpx::threads::thread_priority_critical,
                                                  hpx::threads::thread_stacksize_huge);
    hpx::apply(exe, &Workstealing::Scheduler::scheduler, hpx::util::bind(FuncToCall::fn_ptr(), info, depth, shared_state, prom, nextId, this->get_id()));
  }
  HPX_DEFINE_COMPONENT_ACTION(SearchManager, addWork);

  // Signal the searchManager that a local thread is now finished working and should be removed from active
  void done(unsigned activeId) {
    std::lock_guard<MutexT> l(mtx);
    if (active.find(activeId) != active.end()) {
      active.erase(activeId);
    } else {
      // A steal must be in progress on this id so cancel it before finishing
      // Canceled steals (-1 flag) are already removed from active
      auto & state = inactive[activeId];
      std::vector<Task_t> noSteal {hpx::util::make_tuple(SearchInfo(), -1, hpx::find_here())};
      std::get<1>(*state).set(noSteal);
      inactive.erase(activeId);
    }
    activeIds.push(activeId);
  }

  // Generate a new stealRequest pair that can be used with an existing thread to add steals to it
  // Used for master-threads initialising work while maintaining a stack
  std::pair<std::shared_ptr<SharedState_t>, unsigned> registerThread() {
    std::lock_guard<MutexT> l(mtx);
    auto shared_state = std::make_shared<SharedState_t>();
    auto nextId = activeIds.front();
    activeIds.pop();
    active[nextId] = shared_state;
    return std::make_pair(shared_state, nextId);
  }

  std::vector<hpx::naming::id_type> getAllSearchManagers() {
    std::lock_guard<MutexT> l(mtx);
    std::vector<hpx::naming::id_type> res(distributedSearchManagers);
    res.push_back(this->get_id());
    return res;
  }

  // Called on each node to set the scheduler policy pointer
  // managerId must exist on the locality this is called on
  static void setLocalPolicy(hpx::naming::id_type managerId) {
    Workstealing::Scheduler::local_policy = hpx::get_ptr<SearchManager<SearchInfo, FuncToCall> >(managerId).get();
  }
  struct setLocalPolicy_act : hpx::actions::make_action<
    decltype(&SearchManager<SearchInfo, FuncToCall>::setLocalPolicy),
    &SearchManager<SearchInfo, FuncToCall>::setLocalPolicy,
    setLocalPolicy_act>::type {};

  // Helper function to setup the components/policies on each node and register required information
  static void initPolicy(bool stealAll) {
    std::vector<hpx::naming::id_type> searchManagers;
    for (auto const& loc : hpx::find_all_localities()) {
      auto searchManager = hpx::new_<SearchManager<SearchInfo, FuncToCall> >(loc, stealAll).get();
      hpx::async<setLocalPolicy_act>(loc, searchManager).get();
      searchManagers.push_back(searchManager);
    }

    for (auto const & loc : searchManagers) {
      typedef typename SearchManager<SearchInfo, FuncToCall>::registerDistributedManagers_action registerManagersAct;
      hpx::async<registerManagersAct>(loc, searchManagers).get();
    }
  }

};

}}


#define REGISTER_SEARCHMANAGER(searchInfo,Func)                                                                            \
  typedef Workstealing::Policies::SearchManager<searchInfo, Func > BOOST_PP_CAT(__searchmgr_type_, BOOST_PP_CAT(searchInfo, Func)); \
                                                                                                                           \
  HPX_REGISTER_ACTION(BOOST_PP_CAT(__searchmgr_type_, BOOST_PP_CAT(searchInfo, Func))::registerDistributedManagers_action, \
                      BOOST_PP_CAT(__searchmgr_registerDistributedManagers_action,                                         \
                                   BOOST_PP_CAT(searchInfo, Func)));                                                       \
                                                                                                                           \
  HPX_REGISTER_ACTION(BOOST_PP_CAT(__searchmgr_type_, BOOST_PP_CAT(searchInfo, Func))::addWork_action,                     \
                      BOOST_PP_CAT(__searchmgr_addWork_action,                                                             \
                                   BOOST_PP_CAT(searchInfo, Func)));                                                       \
                                                                                                                           \
  HPX_REGISTER_ACTION(BOOST_PP_CAT(__searchmgr_type_, BOOST_PP_CAT(searchInfo, Func))::getDistributedWork_action,          \
                      BOOST_PP_CAT(__searchmgr_getDistributedWork_action,                                                  \
                                   BOOST_PP_CAT(searchInfo, Func)));                                                       \
                                                                                                                           \
  typedef ::hpx::components::component<BOOST_PP_CAT(__searchmgr_type_, BOOST_PP_CAT(searchInfo, Func)) >                   \
  BOOST_PP_CAT(__searchmgr_, BOOST_PP_CAT(searchInfo, Func));                                                              \
                                                                                                                           \
  HPX_REGISTER_COMPONENT(BOOST_PP_CAT(__searchmgr_,                                                                        \
                                      BOOST_PP_CAT(searchInfo, Func)));                                                    \

#endif
