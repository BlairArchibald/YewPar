#ifndef YEWPAR_POLICY_WORKPOOL_HPP
#define YEWPAR_POLICY_WORKPOOL_HPP

#include "Policy.hpp"
#include "hpx/lcos/local/mutex.hpp"
#include "hpx/util/function.hpp"
#include "../workqueue.hpp"
#include "util/util.hpp"

#include <random>
#include <vector>

namespace Workstealing { namespace Policies {

// TODO: Factor into a .cpp file (there's no templates so this is fine)
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
  Workpool(hpx::naming::id_type localQueue) {
    local_workqueue = localQueue;
    last_remote = hpx::find_here();

    std::random_device rd;
    randGenerator.seed(rd());
  }

  hpx::util::function<void(), false> getWork() override {
    std::unique_lock<mutex_t> l(mtx);

    hpx::util::function<void(hpx::naming::id_type)> task;
    task = hpx::async<workstealing::workqueue::getLocal_action>(local_workqueue).get();

    if (task) {
      return hpx::util::bind(task, hpx::find_here());
    }

    if (!distributed_workqueues.empty()) {
      if (last_remote != hpx::find_here()) {
        task = hpx::async<workstealing::workqueue::steal_action>(last_remote).get();
        if (task) {
          return hpx::util::bind(task, hpx::find_here());
        }
      } else {
        std::uniform_int_distribution<int> rand(0, distributed_workqueues.size() - 1);
        auto victim = distributed_workqueues.begin();
        std::advance(victim, rand(randGenerator));
        task = hpx::async<workstealing::workqueue::steal_action>(*victim).get();

        if (task) {
          last_remote = *victim;
          return hpx::util::bind(task, hpx::find_here());
        }
      }
    }
    return nullptr;
  }

  void addwork(hpx::util::function<void(hpx::naming::id_type)> task) {
    std::unique_lock<mutex_t> l(mtx);
    hpx::apply<workstealing::workqueue::addWork_action>(local_workqueue, task);
  }

  void registerDistributedWorkqueues(std::vector<hpx::naming::id_type> workqueues) {
    std::unique_lock<mutex_t> l(mtx);
    distributed_workqueues = workqueues;
    distributed_workqueues.erase(
        std::remove_if(distributed_workqueues.begin(), distributed_workqueues.end(), YewPar::util::isColocated),
        distributed_workqueues.end());
  }

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
