#ifndef YEWPAR_POLICY_WORKPOOL_HPP
#define YEWPAR_POLICY_WORKPOOL_HPP

#include "Policy.hpp"

#include <hpx/include/components.hpp>

#include <hpx/lcos/async.hpp>
#include <hpx/lcos/broadcast.hpp>
#include <hpx/lcos/local/mutex.hpp>
#include <hpx/util/function.hpp>
#include <hpx/runtime/actions/plain_action.hpp>
#include <hpx/runtime/actions/basic_action.hpp>

#include "../workqueue.hpp"

#include <random>
#include <vector>

namespace Workstealing { namespace Scheduler {extern std::shared_ptr<Policy> local_policy; }}

namespace Workstealing { namespace Policies {

namespace WorkpoolPerf {
void registerPerformanceCounters();
}

// TODO: Performance counters
class Workpool : public Policy {

 private:
  // TODO: Pointer to local_workqueue instead of calling actions from the local version
  hpx::naming::id_type local_workqueue;
  hpx::naming::id_type last_remote;
  std::vector<hpx::naming::id_type> distributed_workqueues;

  // random number generator
  std::mt19937 randGenerator;

  // Policies must be thread safe (TODO: Push to PolicyBase?)
  using mutex_t = hpx::lcos::local::mutex;
  mutex_t mtx;

 public:
  Workpool(hpx::naming::id_type localQueue);
  ~Workpool() = default;

  hpx::util::function<void(), false> getWork() override;

  void addwork(hpx::util::function<void(hpx::naming::id_type)> task);

  void registerDistributedWorkqueues(std::vector<hpx::naming::id_type> workqueues);

  static void setWorkqueue(hpx::naming::id_type localWorkqueue) {
    Workstealing::Scheduler::local_policy = std::make_shared<Workpool>(localWorkqueue);
  }
  struct setWorkqueue_act : hpx::actions::make_action<
    decltype(&Workpool::setWorkqueue),
    &Workpool::setWorkqueue,
    setWorkqueue_act>::type {};

  static void setDistributedWorkqueues(std::vector<hpx::naming::id_type> workqueues) {
    std::static_pointer_cast<Workstealing::Policies::Workpool>(Workstealing::Scheduler::local_policy)->registerDistributedWorkqueues(workqueues);
  }
  struct setDistributedWorkqueues_act : hpx::actions::make_action<
    decltype(&Workpool::setDistributedWorkqueues),
    &Workpool::setDistributedWorkqueues,
    setDistributedWorkqueues_act>::type {};

  static void initPolicy() {
    std::vector<hpx::future<void> > futs;
    std::vector<hpx::naming::id_type> workqueues;
    for (auto const& loc : hpx::find_all_localities()) {
      auto workqueue = hpx::new_<workstealing::workqueue>(loc).get();
      futs.push_back(hpx::async<setWorkqueue_act>(loc, workqueue));
      workqueues.push_back(workqueue);
    }
    hpx::wait_all(futs);
    hpx::wait_all(hpx::lcos::broadcast<setDistributedWorkqueues_act>(hpx::find_all_localities(), workqueues));
  }
};

}}

#endif
