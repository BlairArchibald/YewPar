#ifndef YEWPAR_DOUBLE_WRITE_PROMISE_HPP
#define YEWPAR_DOUBLE_WRITE_PROMISE_HPP

#include <hpx/runtime/actions/component_action.hpp>
#include <hpx/runtime/components/server/component_base.hpp>
#include <hpx/runtime/components/server/locking_hook.hpp>
#include <hpx/runtime/components/server/component.hpp>
#include <hpx/include/lcos.hpp>

// Wraps a promise object (by id) and ensures multiple writes to the promise are
// ignored Useful for creating racing tasks and global flags. Calling code
// should keep track of the wrapped promises future to retrieve the result
namespace YewPar { namespace util {

template <typename T>
class DoubleWritePromise : public hpx::components::locking_hook<
  hpx::components::component_base<DoubleWritePromise<T>> > {

private:
  // Id of the wrapped promise
  hpx::naming::id_type promId;
  bool written;

public:
  DoubleWritePromise() : written(false) {};
  DoubleWritePromise(hpx::naming::id_type prom) : promId (prom), written(false) {};

  bool set_value(T t) {
    if (!written) {
      typedef typename hpx::lcos::base_lco_with_value<T>::set_value_action setAct;
      hpx::async<setAct>(promId, std::move(t)).get();
      written = true;
      return true;
    }
    return false;
    // Ignore multiple writes
  }
  HPX_DEFINE_COMPONENT_ACTION(DoubleWritePromise, set_value);

};

}}

#define REGISTER_DOUBLE_WRITE_PROMISE(type)                                   \
  HPX_REGISTER_ACTION(                                                        \
    YewPar::util::DoubleWritePromise<type>::set_value_action,                 \
    BOOST_PP_CAT(__DoubleWritePromise_set_value_action_, type));              \
                                                                              \
  typedef ::hpx::components::component<YewPar::util::DoubleWritePromise<type> \
    > BOOST_PP_CAT(__DoubleWritePromise_, type);                              \
                                                                              \
  HPX_REGISTER_COMPONENT(BOOST_PP_CAT(__DoubleWritePromise_, type))           \

// // Some common types
// TODO: Check this still works after moving to .cpp
// REGISTER_DOUBLE_WRITE_PROMISE(bool)
// REGISTER_DOUBLE_WRITE_PROMISE(int)

#endif
