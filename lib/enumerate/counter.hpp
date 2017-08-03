#ifndef ENUM_COUNTER_HPP
#define ENUM_COUNTER_HPP

#include <hpx/hpx.hpp>
#include <hpx/include/components.hpp>

#include <cstdint>
#include <map>

namespace skeletons { namespace Enum { namespace Components {
  class Counter : public hpx::components::locking_hook<
    hpx::components::component_base<Counter> > {
    private:
      // Map TreeDepth -> Count
      std::map<unsigned, std::uint64_t> countMap;
    public:
      // TODO: Is a map serializable?
      void add(std::map<unsigned, std::uint64_t> cnt) {
        for (auto const &elm : cnt) {
          if (countMap[elm.first]) {
            countMap[elm.first] += elm.second;
          } else{
            countMap[elm.first] = elm.second;
          }
        }
      }
      HPX_DEFINE_COMPONENT_ACTION(Counter, add);

      std::map<unsigned, std::uint64_t> getCountMap() const {
        return countMap;
      }
      HPX_DEFINE_COMPONENT_ACTION(Counter, getCountMap);
    };
}}}

HPX_REGISTER_ACTION_DECLARATION(skeletons::Enum::Components::Counter::add_action, enum_add_action);
HPX_REGISTER_ACTION_DECLARATION(skeletons::Enum::Components::Counter::getCountMap_action, enum_getCountMap_action);

#endif
