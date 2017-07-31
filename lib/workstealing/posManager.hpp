#ifndef POSMANAGER_COMPONENT_HPP
#define POSMANAGER_COMPONENT_HPP

#include <hpx/hpx.hpp>
#include <hpx/include/components.hpp>
#include <hpx/include/serialization.hpp>
#include <memory>

#include "bnb/positionIndex.hpp"

namespace workstealing { namespace indexed {

    class posManager : public hpx::components::locking_hook<
      hpx::components::component_base<posManager > > {
    private:
      std::vector<std::shared_ptr<positionIndex> > active; // Active position indices for stealing

      using funcType = hpx::util::function<void(const std::shared_ptr<positionIndex>,
                                                const hpx::naming::id_type,
                                                const int,
                                                const hpx::naming::id_type)>;
      std::unique_ptr<funcType> fn;

    public:
      posManager() {};
      posManager(std::unique_ptr<funcType> f) : fn(std::move(f)) {};

      bool getWork() {
        if (active.empty()) {
          return false;
        }

        auto victim = active.begin();
        std::advance(victim, std::rand () % active.size());

        auto p = (*victim)->steal();
        auto path = std::get<0>(p);
        auto prom = std::get<1>(p);

        if (path.empty()) {
          return false;
        } else {
          // Build the action
          auto posIdx = std::make_shared<positionIndex>(path);
          active.push_back(posIdx);

          hpx::threads::executors::current_executor scheduler;
          scheduler.add(hpx::util::bind(*fn, posIdx, prom, active.size() - 1, this->get_id()));

          // How do we know when we can remove this from active, a future with callback?
          return true;
        }
      }
      HPX_DEFINE_COMPONENT_ACTION(posManager, getWork);

      void addWork(std::vector<unsigned> path, hpx::naming::id_type prom) {
        auto posIdx = std::make_shared<positionIndex>(path);
        active.push_back(posIdx);
        hpx::threads::executors::current_executor scheduler;
        scheduler.add(hpx::util::bind(*fn, posIdx, prom, active.size() - 1, this->get_id()));
      }
      HPX_DEFINE_COMPONENT_ACTION(posManager, addWork);

      void done(int pos) {
        active.erase(active.begin() + pos);
      }
      HPX_DEFINE_COMPONENT_ACTION(posManager, done);
    };
}}

HPX_REGISTER_ACTION_DECLARATION(workstealing::indexed::posManager::getWork_action, posManager_getWork_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::indexed::posManager::addWork_action, posManager_addWork_action);
HPX_REGISTER_ACTION_DECLARATION(workstealing::indexed::posManager::done_action, posManager_done_action);


#endif
