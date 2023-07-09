#ifndef PRIORITY_WORKQUEUE_COMPONENT_HPP
#define PRIORITY_WORKQUEUE_COMPONENT_HPP

#include <queue>
#include <vector>

#include <hpx/include/components.hpp>
#include <hpx/functional/function.hpp>

namespace workstealing
{
  class PriorityWorkqueue : public hpx::components::locking_hook<
    hpx::components::component_base<PriorityWorkqueue>
    >
    {
    private:
        using funcType  = hpx::distributed::function<void(hpx::id_type)>;
        using queueType = hpx::tuple<int, funcType>;

        struct PriorityWorkqueueCompare {
          bool operator()(queueType a, queueType b) {
            auto i1 = hpx::get<0>(a);
            auto i2 = hpx::get<0>(b);
            return i1 < i2;
          }
        };

        std::priority_queue<queueType, std::vector<queueType>, PriorityWorkqueueCompare> tasks;

      public:
        funcType steal();
        HPX_DEFINE_COMPONENT_ACTION(PriorityWorkqueue, steal);
        void addWork(int priority, funcType task);
        HPX_DEFINE_COMPONENT_ACTION(PriorityWorkqueue, addWork);
        bool workRemaining();
        HPX_DEFINE_COMPONENT_ACTION(PriorityWorkqueue, workRemaining);
  };
}

HPX_REGISTER_ACTION_DECLARATION(workstealing::PriorityWorkqueue::steal_action, workqueue_prio_steal_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::PriorityWorkqueue::addWork_action, workqueue_prio_addWork_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::PriorityWorkqueue::workRemaining_action, workqueue_prio_workRemaining_action);

#endif
