#ifndef BNB_REGISTRY_HPP
#define BNB_REGISTRY_HPP

#include <atomic>

#include <hpx/runtime/naming/id_type.hpp>
#include <hpx/runtime/actions/plain_action.hpp>

namespace skeletons { namespace BnB { namespace Components {

      template <typename Space, typename Sol, typename Bnd, typename Cand>
      struct Registry {
        static Registry<Space, Sol, Bnd, Cand>* gReg;

        Space space_;
        std::atomic<Bnd> localBound_;
        hpx::naming::id_type globalIncumbent_;

        // For decision based problems
        std::atomic<bool> stopSearch_ {false};

        // For recompute we need to store the root
        hpx::util::tuple<Sol, Bnd, Cand> root_;
      };

      template<typename Space, typename Sol, typename Bnd, typename Cand>
      Registry<Space, Sol, Bnd, Cand>* Registry<Space, Sol, Bnd, Cand>::gReg = new Registry<Space, Sol, Bnd, Cand>;

      template <typename Space, typename Sol, typename Bnd, typename Cand>
      void initialiseRegistry(Space space, Bnd bnd, hpx::naming::id_type inc, hpx::util::tuple<Sol, Bnd, Cand> root) {
        auto reg = Registry<Space, Sol, Bnd, Cand>::gReg;
        reg->space_ = space;
        reg->localBound_ = bnd;
        reg->globalIncumbent_ = inc;
        reg->root_ = root;
      }

      template <typename Space, typename Sol, typename Bnd, typename Cand>
      void updateRegistryBound(Bnd bnd) {
        auto reg = Registry<Space, Sol, Bnd, Cand>::gReg;
        while (true) {
          auto curBound = reg->localBound_.load();
          if (bnd < curBound) {
            break;
          }

          if (reg->localBound_.compare_exchange_weak(curBound, bnd)) {
            break;
          }
        }
      }

      // FIXME: Fake arg needed for the action to find the correct function. Not sure why.
      template <typename Space, typename Sol, typename Bnd, typename Cand>
      void setStopSearchFlag(Bnd fake) {
        auto reg = Registry<Space, Sol, Bnd, Cand>::gReg;
        reg->stopSearch_.store(true);
      }
}}}

HPX_DECLARE_PLAIN_ACTION(skeletons::BnB::Components::initialiseRegistry, initRegistry_act);
HPX_DECLARE_PLAIN_ACTION(skeletons::BnB::Components::updateRegistryBound, updateRegistryBound_act);
HPX_DECLARE_PLAIN_ACTION(skeletons::BnB::Components::setStopSearchFlag, setStopSearchFlag_act);

#define COMMA ,
#define REGISTER_REGISTRY(space,sol,bnd,cand)                                                          \
  struct initRegistry_act : hpx::actions::make_action<                                                 \
    decltype(&skeletons::BnB::Components::initialiseRegistry<space COMMA sol COMMA bnd COMMA cand >),  \
    &skeletons::BnB::Components::initialiseRegistry<space COMMA sol COMMA bnd COMMA cand >,            \
    initRegistry_act>::type {};                                                                        \
                                                                                                       \
  HPX_REGISTER_ACTION_DECLARATION(initRegistry_act, initRegistry_act);                                 \
  HPX_REGISTER_ACTION(initRegistry_act, initRegistry_act);                                             \
                                                                                                       \
  struct updateRegistryBound_act : hpx::actions::make_action<                                          \
    decltype(&skeletons::BnB::Components::updateRegistryBound<space COMMA sol COMMA bnd COMMA cand >), \
    &skeletons::BnB::Components::updateRegistryBound<space COMMA sol COMMA bnd COMMA cand >,           \
    updateRegistryBound_act>::type {};                                                                 \
                                                                                                       \
  HPX_REGISTER_ACTION_DECLARATION(updateRegistryBound_act, updateRegistryBound_act);                   \
  HPX_REGISTER_ACTION(updateRegistryBound_act, updateRegistryBound_act);                               \
                                                                                                       \
  struct setStopSearchFlag_act : hpx::actions::make_action<                                            \
    decltype(&skeletons::BnB::Components::setStopSearchFlag<space COMMA sol COMMA bnd COMMA cand >),   \
      &skeletons::BnB::Components::setStopSearchFlag<space COMMA sol COMMA bnd COMMA cand >,           \
      setStopSearchFlag_act>::type {};                                                                 \
                                                                                                       \
  HPX_REGISTER_ACTION_DECLARATION(setStopSearchFlag_act, setStopSearchFlag_act);                       \
  HPX_REGISTER_ACTION(setStopSearchFlag_act, setStopSearchFlag_act);                                   \

#endif
