#ifndef ENUM_REGISTRY_HPP
#define ENUM_REGISTRY_HPP

#include <unordered_map>
#include <hpx/runtime/serialization/unordered_map.hpp>

namespace skeletons { namespace Enum { namespace Components {

      template <typename Space, typename Sol>
      struct Registry {
        static Registry<Space, Sol>* gReg;

        Space space_;

        unsigned maxDepth;
        std::unordered_map<unsigned, std::atomic<uint64_t> > counts;

        // For recompute we need to store the root
        Sol root_;

        void updateCounts(std::unordered_map<unsigned, uint64_t> & cntMap) {
          for (auto const &elm : cntMap) {
            // Addition happens atomically
            counts[elm.first] += elm.second;
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
        for (auto i = 0; i <= maxDepth; ++i) {
          reg->counts[i] = 0;
        }
      }

      // Faketypes needed so we can typecheck before full action initialisation
      template <typename Space, typename Sol>
      std::unordered_map<unsigned, uint64_t> getCounts(Space fake1, Sol fake2) {
        auto reg = Registry<Space, Sol>::gReg;
        std::unordered_map<unsigned, uint64_t> res;

        // Convert std::atomic<uint64_t> -> uint64_t by loading it
        for (const auto & elm : reg->counts) {
          res[elm.first] = elm.second.load();
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
