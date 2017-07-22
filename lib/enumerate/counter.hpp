#ifndef ENUN_COUNTER_HPP
#define ENUN_COUNTER_HPP

#include <hpx/hpx.hpp>
#include <hpx/include/components.hpp>

#include <hpx/util/tuple.hpp>
#include <cstdint>

namespace Enum {
  class Counter : public hpx::components::locking_hook<
    hpx::components::component_base<Counter> > {
    private:
      std::uint64_t count = 0;
    public:
      void add(std::uint64_t cnt) {
        count += cnt;
      }
      HPX_DEFINE_COMPONENT_ACTION(Counter, add);

      std::uint64_t getCount() const {
        return count;
      }
      HPX_DEFINE_COMPONENT_ACTION(Counter, getCount);
    };
}

HPX_REGISTER_ACTION_DECLARATION(Enum::Counter::add_action, enum_add_action);
HPX_REGISTER_ACTION_DECLARATION(Enum::Counter::getCount_action, enum_getCount_action);

#endif
