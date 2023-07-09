#ifndef DEPTHPOOL_COMPONENT_HPP
#define DEPTHPOOL_COMPONENT_HPP

#include <queue>

#include <hpx/include/components.hpp>
#include <hpx/functional/function.hpp>

namespace workstealing {

// A workqueue that tracks tasks based on the depth in the tree they were created at.
// This allows high vs low tasks to be distinguished while maintaining heuristics as much as possible.
// In particular a sequential user should see tasks in the same order as a sequential thread
// Currently only supports access by a single thread at a time as it's very non trivial to implement lock-free.
class DepthPool : public hpx::components::locking_hook<
  hpx::components::component_base<DepthPool>> {
 private:
  using fnType = hpx::distributed::function<void(hpx::id_type)>;

  std::vector< std::queue<fnType> > pools;

  // For quicker access
  unsigned lowest = 0;
  unsigned max_depth;

 public:
  DepthPool() {
    // TODO: Size should be settable/dynamic. Currently the same as the default max_depth
    max_depth = 5000;
    pools.resize(max_depth);
  }

  fnType getLocal();
  HPX_DEFINE_COMPONENT_ACTION(DepthPool, getLocal);
  fnType steal();
  HPX_DEFINE_COMPONENT_ACTION(DepthPool, steal);
  void addWork(fnType task, unsigned depth);
  HPX_DEFINE_COMPONENT_ACTION(DepthPool, addWork);
};
}

HPX_REGISTER_ACTION_DECLARATION(workstealing::DepthPool::getLocal_action, DepthPool_getLocal_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::DepthPool::steal_action, DepthPool_steal_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::DepthPool::addWork_action, DepthPool_addWork_action);

#endif
