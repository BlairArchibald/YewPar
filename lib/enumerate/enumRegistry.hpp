#ifndef ENUM_REGISTRY_HPP
#define ENUM_REGISTRY_HPP

namespace skeletons { namespace Enum { namespace Components {

      template <typename Space, typename Sol>
      struct Registry {
        static Registry<Space, Sol>* gReg;

        Space space_;
        hpx::naming::id_type globalCounter;

        // For recompute we need to store the root
        Sol root_;
      };

      template<typename Space, typename Sol>
      Registry<Space, Sol>* Registry<Space, Sol>::gReg = new Registry<Space, Sol>;

      template <typename Space, typename Sol>
      void initialiseRegistry(Space space, hpx::naming::id_type counter, Sol root) {
        auto reg = Registry<Space, Sol>::gReg;
        reg->space_ = space;
        reg->globalCounter = counter;
      }
}}}

HPX_DECLARE_PLAIN_ACTION(skeletons::Enum::Components::initialiseRegistry, enum_initRegistry_act);

#define COMMA ,
#define REGISTER_ENUM_REGISTRY(space,sol)                                        \
  struct enum_initRegistry_act : hpx::actions::make_action<                      \
    decltype(&skeletons::Enum::Components::initialiseRegistry<space COMMA sol>), \
    &skeletons::Enum::Components::initialiseRegistry<space COMMA sol>,           \
    enum_initRegistry_act>::type {};                                             \
                                                                                 \
  HPX_REGISTER_ACTION_DECLARATION(enum_initRegistry_act, enum_initRegistry_act); \
  HPX_REGISTER_ACTION(enum_initRegistry_act, enum_initRegistry_act);             \

#endif
