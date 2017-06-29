#ifndef POSMANAGER_COMPONENT_HPP
#define POSMANAGER_COMPONENT_HPP

#include <hpx/hpx.hpp>
#include <hpx/include/components.hpp>
#include <memory>

#include "bnb/positionIndex.hpp"

namespace workstealing { namespace indexed {

    class posManager : public hpx::components::locking_hook<
      hpx::components::component_base<posManager> > {
    private:
      std::vector<std::shared_ptr<positionIndex> > active; // Active position indices for stealing

    public:
      posManager() {};

      template <typename Fn>
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
          scheduler.add(hpx::util::bind(Fn(), posIdx, prom));

          // How do we know when we can remove this from active, a future with callback?
          return true;
        }
      }
      template <typename T>
      struct getWork_action
        : hpx::actions::make_action<
          decltype(&getWork<T>),
          &getWork<T>,
          getWork_action<T> > {};
    };
}}

#endif
