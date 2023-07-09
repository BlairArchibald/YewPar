#ifndef YEWPAR_POLICY_WORKPOOL_HPP
#define YEWPAR_POLICY_WORKPOOL_HPP

#include "Policy.hpp"

#include <hpx/runtime_distributed/find_all_localities.hpp>
#include <hpx/modules/collectives.hpp>

#include "workstealing/Workqueue.hpp"

#include <random>
#include <vector>

namespace Workstealing { namespace Scheduler {extern std::shared_ptr<Policy> local_policy; }}

namespace Workstealing { namespace Policies {

namespace WorkpoolPerf {
void registerPerformanceCounters();
}

class Workpool : public Policy {

 private:
  // TODO: Pointer to local_workqueue instead of calling actions from the local version
  hpx::id_type local_workqueue;
  hpx::id_type last_remote;
  std::vector<hpx::id_type> distributed_workqueues;

  // random number generator
  std::mt19937 randGenerator;

  // Policies must be thread safe (TODO: Push to PolicyBase?)
  using mutex_t = hpx::mutex;
  mutex_t mtx;

 public:
  Workpool(hpx::id_type localQueue);
  ~Workpool() = default;

  hpx::function<void(), false> getWork() override;

  void addwork(hpx::distributed::function<void(hpx::id_type)> task);

  void registerDistributedWorkqueues(std::vector<hpx::id_type> workqueues);

  static void setWorkqueue(hpx::id_type localWorkqueue) {
    Workstealing::Scheduler::local_policy = std::make_shared<Workpool>(localWorkqueue);
  }
  struct setWorkqueue_act : hpx::actions::make_action<
    decltype(&Workpool::setWorkqueue),
    &Workpool::setWorkqueue,
    setWorkqueue_act>::type {};

  static void setDistributedWorkqueues(std::vector<hpx::id_type> workqueues) {
    std::static_pointer_cast<Workstealing::Policies::Workpool>(Workstealing::Scheduler::local_policy)->registerDistributedWorkqueues(workqueues);
  }
  struct setDistributedWorkqueues_act : hpx::actions::make_action<
    decltype(&Workpool::setDistributedWorkqueues),
    &Workpool::setDistributedWorkqueues,
    setDistributedWorkqueues_act>::type {};

  static void initPolicy() {
    std::vector<hpx::future<void> > futs;
    std::vector<hpx::id_type> workqueues;
    for (auto const& loc : hpx::find_all_localities()) {
      auto workqueue = hpx::new_<workstealing::Workqueue>(loc).get();
      futs.push_back(hpx::async<setWorkqueue_act>(loc, workqueue));
      workqueues.push_back(workqueue);
    }
    hpx::wait_all(futs);
    hpx::wait_all(hpx::lcos::broadcast<setDistributedWorkqueues_act>(hpx::find_all_localities(), workqueues));
  }
};

}}

#endif
