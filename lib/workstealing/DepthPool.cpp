#include "DepthPool.hpp"

#include <hpx/include/components.hpp>

namespace workstealing {

DepthPool::fnType DepthPool::steal() {
  DepthPool::fnType task;

  for (auto i = 0; i <= lowest; ++i) {
    if (!pools[i].empty()) {
      auto task = pools[i].front();
      pools[i].pop();
      return task;
    }
  }
  return nullptr;
}

DepthPool::fnType DepthPool::getLocal() {
  DepthPool::fnType task;
  if (pools[lowest].empty()) {
    return nullptr;
  } else {
    task = pools[lowest].front();
    pools[lowest].pop();
  }

  // Update lowest pointer if required
  while (lowest > 0) {
    if (pools[lowest].empty()) {
        --lowest;
    } else {
      break;
    }
  }
}

void DepthPool::addWork(DepthPool::fnType task, unsigned depth) {
  pools[depth].push(task);

  if (depth > lowest) {
    lowest = depth;
  }
}

}

HPX_REGISTER_COMPONENT_MODULE();

typedef hpx::components::component<workstealing::DepthPool> DepthPool_type;

HPX_REGISTER_COMPONENT(DepthPool_type, DepthPool);

HPX_REGISTER_ACTION(workstealing::DepthPool::getLocal_action, DepthPool_getLocal_action);
HPX_REGISTER_ACTION(workstealing::DepthPool::steal_action, DepthPool_steal_action);
HPX_REGISTER_ACTION(workstealing::DepthPool::addWork_action, DepthPool_addWork_action);
