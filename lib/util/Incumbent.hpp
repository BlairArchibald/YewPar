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
template<typename Node>
class Incumbent : public hpx::components::locking_hook<
  hpx::components::component_base<Incumbent<Node> >
  > {
 private:
  Node incumbentNode;
 public:
  void updateIncumbent(Node incumbent) {
    // TODO: Pass the right comparator object
    if (incumbent.getObj() > incumbentNode.getObj()) {
      incumbentNode = incumbent;
    }
  }
  struct UpdateIncumbentAct : hpx::actions::make_action<
    decltype(&Incumbent<Node>::updateIncumbent),
    &Incumbent<Node>::updateIncumbent,
    UpdateIncumbentAct>::type {};

  Node getIncumbent() const {
    return incumbentNode;
  }
  struct GetIncumbentAct : hpx::actions::make_action<
    decltype(&Incumbent<Node>::getIncumbent),
    &Incumbent<Node>::getIncumbent,
    GetIncumbentAct>::type {};
};
}

// Do I still need this?
#define REGISTER_INCUMBENT(node)                            \
  typedef YewPar::Incumbent<node> __incumbent_type_;        \
                                                            \
  typedef ::hpx::components::component<__incumbent_type_ >  \
  BOOST_PP_CAT(__incumbent_, node);                         \
  HPX_REGISTER_COMPONENT(BOOST_PP_CAT(__incumbent_, node))  \

#endif
