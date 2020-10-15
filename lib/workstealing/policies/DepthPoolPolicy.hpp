#ifndef YEWPAR_POLICY_DEPTHPOOL_HPP
#define YEWPAR_POLICY_DEPTHPOOL_HPP

#include "Policy.hpp"

#include <hpx/include/components.hpp>
#include <hpx/collectives/broadcast.hpp>

#include "../DepthPool.hpp"

#include <random>
#include <vector>

namespace Workstealing { namespace Scheduler {extern std::shared_ptr<Policy> local_policy; }}

namespace Workstealing { namespace Policies {

namespace DepthPoolPolicyPerf {
void registerPerformanceCounters();
}


// TODO: Performance counters
class DepthPoolPolicy : public Policy {

 private:
  hpx::naming::id_type local_workpool;
  hpx::naming::id_type last_remote;
  std::vector<hpx::naming::id_type> distributed_workpools;

  // random number generator
  std::mt19937 randGenerator;

  using mutex_t = hpx::lcos::local::mutex;
  mutex_t mtx;

 public:
  DepthPoolPolicy(hpx::naming::id_type workpool);
  ~DepthPoolPolicy() = default;

  hpx::util::function<void(), false> getWork() override;

  void addwork(hpx::util::function<void(hpx::naming::id_type)> task, unsigned depth);

  void registerDistributedDepthPools(std::vector<hpx::naming::id_type> workpools);

  static void setDepthPool(hpx::naming::id_type localworkpool) {
    Workstealing::Scheduler::local_policy = std::make_shared<DepthPoolPolicy>(localworkpool);
  }
  struct setDepthPool_act : hpx::actions::make_action<
    decltype(&DepthPoolPolicy::setDepthPool),
    &DepthPoolPolicy::setDepthPool,
    setDepthPool_act>::type {};

  static void setDistributedDepthPools(std::vector<hpx::naming::id_type> workpools) {
    std::static_pointer_cast<Workstealing::Policies::DepthPoolPolicy>(Workstealing::Scheduler::local_policy)->registerDistributedDepthPools(workpools);
  }
  struct setDistributedDepthPools_act : hpx::actions::make_action<
    decltype(&DepthPoolPolicy::setDistributedDepthPools),
    &DepthPoolPolicy::setDistributedDepthPools,
    setDistributedDepthPools_act>::type {};

  static void initPolicy() {
    std::vector<hpx::future<void> > futs;
    std::vector<hpx::naming::id_type> pools;
    for (auto const& loc : hpx::find_all_localities()) {
      auto depthpool = hpx::new_<workstealing::DepthPool>(loc).get();
      futs.push_back(hpx::async<setDepthPool_act>(loc, depthpool));
      pools.push_back(depthpool);
    }
    hpx::wait_all(futs);
    hpx::wait_all(hpx::lcos::broadcast<setDistributedDepthPools_act>(hpx::find_all_localities(), pools));
  }
};

}}

#endif
