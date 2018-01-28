#ifndef YEWPAR_INCUMBENT_HPP
#define YEWPAR_INCUMBENT_HPP

#include <functional>
#include <memory>

#include <hpx/include/components.hpp>

namespace YewPar {

struct Incumbent : public hpx::components::locking_hook<
  hpx::components::component_base<Incumbent> > {

  // A type erased ptr to the actual component type IncumbentComp This avoids
  // needing a REGISTER_X macro per type as would be required for a templated
  // component. Instead we use templated actions to constrain the type. The API
  // is a little strange, but that's okay since it should only be called form
  // the library, never user code.
  std::unique_ptr<void, std::function<void(void*)> > ptr;

  template <typename Node, typename Bound, typename Cmp>
  void init() {
    std::function<void(void*)> del = [](void * t) {
      auto comp = static_cast<Incumbent::IncumbentComp<Node, Bound, Cmp>*>(t);
      delete comp;
    };
    ptr = std::unique_ptr<void, std::function<void(void*)> >(
        new IncumbentComp<Node, Bound, Cmp>(), del);
  }

  template <typename Node, typename Bound, typename Cmp>
  struct InitComponentAct : hpx::actions::make_action<
    decltype(&Incumbent::init<Node, Bound, Cmp>),
    &Incumbent::init<Node, Bound, Cmp>,
    InitComponentAct<Node, Bound, Cmp> >::type {};

  template<typename Node, typename Bound, typename Cmp>
  class IncumbentComp {
   private:
    Node incumbentNode;
    Bound bnd;

  public:
    void initialiseIncumbent(Node n, Bound b) {
      incumbentNode = n;
      bnd = b;
    }

    void updateIncumbent(Node incumbent) {
      Cmp cmp;
      if (cmp(incumbent.getObj(), incumbentNode.getObj())) {
        incumbentNode = incumbent;
        bnd = incumbent.getObj();
      }
    }

    Node getIncumbent() const {
      return incumbentNode;
    }
  };

  template<typename Node, typename Bound, typename Cmp>
  void initialiseIncumbent(Node n, Bound b) {
    auto p = ptr.get();
    auto cmp = static_cast<Incumbent::IncumbentComp<Node, Bound, Cmp>*>(p);
    cmp->initialiseIncumbent(n, b);
  }

  template<typename Node, typename Bound, typename Cmp>
  void updateIncumbent(Node incumbent) {
    auto p = ptr.get();
    auto cmp = static_cast<Incumbent::IncumbentComp<Node, Bound, Cmp>*>(p);
    cmp->updateIncumbent(incumbent);
  }

  template<typename Node, typename Bound, typename Cmp>
  Node getIncumbent() const {
    auto p = ptr.get();
    auto cmp = static_cast<Incumbent::IncumbentComp<Node, Bound, Cmp>*>(p);
    return cmp->getIncumbent();
  }

  template<typename Node, typename Bound, typename Cmp>
  struct UpdateIncumbentAct : hpx::actions::make_action<
    decltype(&Incumbent::updateIncumbent<Node, Bound, Cmp>),
    &Incumbent::updateIncumbent<Node, Bound, Cmp>,
    UpdateIncumbentAct<Node, Bound, Cmp> >::type {};

  template<typename Node, typename Bound, typename Cmp>
  struct InitialiseIncumbentAct : hpx::actions::make_action<
    decltype(&Incumbent::initialiseIncumbent<Node, Bound, Cmp>),
    &Incumbent::initialiseIncumbent<Node, Bound, Cmp>,
    InitialiseIncumbentAct<Node, Bound, Cmp> >::type {};

  template<typename Node, typename Bound, typename Cmp>
  struct GetIncumbentAct : hpx::actions::make_action<
    decltype(&Incumbent::getIncumbent<Node, Bound, Cmp>),
    &Incumbent::getIncumbent<Node, Bound, Cmp>,
    GetIncumbentAct<Node, Bound, Cmp> >::type {};
};

}

typedef hpx::components::component<YewPar::Incumbent> incumbent_comp;
HPX_REGISTER_COMPONENT(incumbent_comp);

#endif
