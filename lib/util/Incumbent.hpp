#ifndef YEWPAR_INCUMBENT_HPP
#define YEWPAR_INCUMBENT_HPP

#include <boost/preprocessor/cat.hpp>                      // for BOOST_PP_CAT
#include <hpx/util/tuple.hpp>                              // for tuple, get
#include "hpx/runtime/actions/basic_action.hpp"            // for HPX_REGIST...
#include "hpx/runtime/actions/component_action.hpp"        // for HPX_DEFINE...
#include "hpx/runtime/components/component_factory.hpp"    // for HPX_REGIST...
#include "hpx/runtime/components/server/locking_hook.hpp"  // for locking_hook
namespace hpx { namespace components { template <typename Component> class component_base; } }

namespace YewPar {
// FIXME: We probably don't need to store the candidate search space
template<typename Node, typename Bound>
class Incumbent : public hpx::components::locking_hook<
  hpx::components::component_base<Incumbent<Node, Bound> >
  > {
 private:
  Node incumbentNode;
  Bound bnd;
 public:

  void initialiseIncumbent(Node n, Bound b) {
    incumbentNode = n;
    bnd = b;
  }
  struct InitialiseIncumbentAct : hpx::actions::make_action<
    decltype(&Incumbent<Node, Bound>::initialiseIncumbent),
    &Incumbent<Node, Bound>::initialiseIncumbent,
    InitialiseIncumbentAct>::type {};

  template <typename Cmp = std::greater<> >
  void updateIncumbent(Node incumbent) {
    Cmp cmp;
    if (cmp(incumbent.getObj(), incumbentNode.getObj())) {
      incumbentNode = incumbent;
      bnd = incumbent.getObj();
    }
  }

  template <typename Cmp = std::greater<> >
  struct UpdateIncumbentAct : hpx::actions::make_action<
    decltype(&Incumbent<Node, Bound>::updateIncumbent<Cmp>),
    &Incumbent<Node, Bound>::updateIncumbent<Cmp>,
    UpdateIncumbentAct<Cmp> >::type {};

  Node getIncumbent() const {
    return incumbentNode;
  }
  struct GetIncumbentAct : hpx::actions::make_action<
    decltype(&Incumbent<Node, Bound>::getIncumbent),
    &Incumbent<Node, Bound>::getIncumbent,
    GetIncumbentAct>::type {};
};
}

#define REGISTER_INCUMBENT(node)     \
  REGISTER_INCUMBENT_BND(node, bool) \

#define REGISTER_INCUMBENT_BND(node, bnd)                                     \
  typedef YewPar::Incumbent<node, bnd > __incumbent_type_bnd;                 \
                                                                              \
  typedef ::hpx::components::component<__incumbent_type_bnd >                 \
  BOOST_PP_CAT(__incumbent_, BOOST_PP_CAT(node, bnd));                        \
  HPX_REGISTER_COMPONENT(BOOST_PP_CAT(__incumbent_, BOOST_PP_CAT(node, bnd))) \

#endif
