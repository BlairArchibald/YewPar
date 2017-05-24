#ifndef PRIORITY_WORKQUEUE_COMPONENT_HPP
#define PRIORITY_WORKQUEUE_COMPONENT_HPP

#include <hpx/hpx.hpp>
#include <hpx/include/components.hpp>
#include <hpx/util/tuple.hpp>

#include <queue>

namespace workstealing
{
  namespace detail {
    struct priorityworkqueueCompare {
      using funcType  = hpx::util::function<void(hpx::naming::id_type)>;
      using queueType = hpx::util::tuple<int, funcType>;

      bool operator()(queueType a, queueType b) {
        auto i1 = hpx::util::get<0>(a);
        auto i2 = hpx::util::get<0>(b);
        return i1 < i2;
      }
    };
  }

  class priorityworkqueue : public hpx::components::locking_hook<
    hpx::components::component_base<priorityworkqueue>
    >
    {
    private:
      using funcType  = hpx::util::function<void(hpx::naming::id_type)>;
      using queueType = hpx::util::tuple<int, funcType>;

      std::priority_queue<queueType, std::vector<queueType>, detail::priorityworkqueueCompare> tasks;

    public:
      funcType steal();
      HPX_DEFINE_COMPONENT_ACTION(priorityworkqueue, steal);
      void addWork(int priority, funcType task);
      HPX_DEFINE_COMPONENT_ACTION(priorityworkqueue, addWork);
    };
}

HPX_REGISTER_ACTION_DECLARATION(workstealing::priorityworkqueue::steal_action, workqueue_steal_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::priorityworkqueue::addWork_action, workqueue_addWork_action);

#endif
