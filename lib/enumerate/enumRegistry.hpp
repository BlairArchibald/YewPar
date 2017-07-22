#ifndef ENUM_REGISTRY_HPP
#define ENUM_REGISTRY_HPP

namespace skeletons { namespace Enum { namespace Components {

      template <typename Space, typename Sol, typename Cand>
      struct Registry {
        static Registry<Space, Sol, Cand>* gReg;

        Space space_;
        hpx::naming::id_type globalCounter;

        // For recompute we need to store the root
        hpx::util::tuple<Sol, Cand> root_;
      };

      template<typename Space, typename Sol, typename Cand>
      Registry<Space, Sol, Cand>* Registry<Space, Sol, Cand>::gReg = new Registry<Space, Sol, Cand>;

      template <typename Space, typename Sol, typename Cand>
      void initialiseRegistry(Space space, hpx::naming::id_type counter, hpx::util::tuple<Sol, Cand> root) {
        auto reg = Registry<Space, Sol, Cand>::gReg;
        reg->space_ = space;
        reg->globlCounter_ = counter;
      }
}}}

HPX_DECLARE_PLAIN_ACTION(skeletons::Enum::Components::initialiseRegistry, enum_initRegistry_act);

#define COMMA ,
#define REGISTER_ENUM_REGISTRY(space,sol,cand)                                              \
  struct initRegistry_act : hpx::actions::make_action<                                      \
    decltype(&skeletons::BnB::Components::initialiseRegistry<space COMMA sol COMMA cand >), \
    &skeletons::BnB::Components::initialiseRegistry<space COMMA sol COMMA cand >,           \
    initRegistry_act>::type {};                                                             \
                                                                                            \
  HPX_REGISTER_ACTION_DECLARATION(enum_initRegistry_act, enum_initRegistry_act);            \
  HPX_REGISTER_ACTION(enum_initRegistry_act, enum_initRegistry_act);                        \

#endif
