#ifndef ENUM_REGISTRY_HPP
#define ENUM_REGISTRY_HPP

#include <vector>

namespace skeletons { namespace Enum { namespace Components {

      template <typename Space, typename Sol>
      struct Registry {
        static Registry<Space, Sol>* gReg;

        Space space_;

        unsigned maxDepth;
        std::vector<std::atomic<std::uint64_t> >* counts;

        // For recompute we need to store the root
        Sol root_;

        void updateCounts(std::vector<std::uint64_t> & cntMap) {
          for (auto i = 0; i <= maxDepth; ++i) {
            // Addition happens atomically
            (*counts)[i] += cntMap[i];
          }
        }
      };

      template<typename Space, typename Sol>
      Registry<Space, Sol>* Registry<Space, Sol>::gReg = new Registry<Space, Sol>;

      template <typename Space, typename Sol>
      void initialiseRegistry(Space space, unsigned maxDepth , Sol root) {
        auto reg = Registry<Space, Sol>::gReg;
        reg->space_ = space;
        reg->maxDepth = maxDepth;
        reg->root_ = root;

        // FIXME: Technically a memory leak
        // FIXME: Should we just move to fixed size arrays for this?
        reg->counts = new std::vector<std::atomic<std::uint64_t>>(maxDepth);
        for (auto i = 0; i <= reg->maxDepth; ++i) {
          (*reg->counts)[i] = 0;
        }
      }

      // Faketypes needed so we can typecheck before full action initialisation
      template <typename Space, typename Sol>
      std::vector<std::uint64_t> getCounts(Space, Sol) {
        auto reg = Registry<Space, Sol>::gReg;
        std::vector<std::uint64_t> res;

        // Convert std::atomic<std::uint64_t> -> uint64_t by loading it
        res.resize(reg->maxDepth + 1);
        for (auto i = 0; i <= reg->maxDepth; ++i) {
          res[i] = (*reg->counts)[i].load();
        }

        return res;
      }
}}}

HPX_DECLARE_PLAIN_ACTION(skeletons::Enum::Components::initialiseRegistry, enum_initRegistry_act);
HPX_DECLARE_PLAIN_ACTION(skeletons::Enum::Components::getCounts, enum_getCounts_act);

#define COMMA ,

#define REGISTER_ENUM_REGISTRY(space,sol)                                        \
  struct enum_initRegistry_act : hpx::actions::make_action<                      \
    decltype(&skeletons::Enum::Components::initialiseRegistry<space COMMA sol>), \
    &skeletons::Enum::Components::initialiseRegistry<space COMMA sol>,           \
    enum_initRegistry_act>::type {};                                             \
                                                                                 \
  HPX_REGISTER_ACTION_DECLARATION(enum_initRegistry_act, enum_initRegistry_act); \
  HPX_REGISTER_ACTION(enum_initRegistry_act, enum_initRegistry_act);             \
                                                                                 \
  struct enum_getCounts_act : hpx::actions::make_action<                         \
    decltype(&skeletons::Enum::Components::getCounts<space COMMA sol>),          \
    &skeletons::Enum::Components::getCounts<space COMMA sol>,                    \
    enum_getCounts_act>::type {};                                                \
                                                                                 \
  HPX_REGISTER_ACTION_DECLARATION(enum_getCounts_act, enum_getCounts_act);       \
  HPX_REGISTER_ACTION(enum_getCounts_act, enum_getCounts_act);                   \

#endif
