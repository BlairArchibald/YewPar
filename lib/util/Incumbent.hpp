#ifndef YEWPAR_INCUMBENT_HPP
#define YEWPAR_INCUMBENT_HPP

#include <functional>
#include <memory>

#include <hpx/include/components.hpp>
#include <hpx/include/iostreams.hpp>

namespace YewPar {

struct Incumbent : public hpx::components::locking_hook<
  hpx::components::component_base<Incumbent> > {

  // A type erased ptr to the actual component type IncumbentComp This avoids
  // needing a REGISTER_X macro per type as would be required for a templated
  // component. Instead we use templated actions to constrain the type. The API
  // is a little strange, but that's okay since it should only be called form
  // the library, never user code.
  std::unique_ptr<void, std::function<void(void*)> > ptr;

  template<typename Node, typename Bound, typename Cmp, typename Verbose>
  void init() {
    std::function<void(void*)> del = [](void * t) {
      auto comp = static_cast<Incumbent::IncumbentComp<Node, Bound, Cmp, Verbose>*>(t);
      delete comp;
    };
    ptr = std::unique_ptr<void, std::function<void(void*)> >(
        new IncumbentComp<Node, Bound, Cmp, Verbose>(), del);
  }

  template <typename Node, typename Bound, typename Cmp, typename Verbose>
  struct InitComponentAct : hpx::actions::make_action<
    decltype(&Incumbent::init<Node, Bound, Cmp, Verbose>),
    &Incumbent::init<Node, Bound, Cmp, Verbose>,
    InitComponentAct<Node, Bound, Cmp, Verbose> >::type {};

  template<typename Node, typename Bound, typename Cmp, typename Verbose>
  class IncumbentComp {
   private:
    static constexpr unsigned verbose = Verbose::value;

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
        if constexpr(verbose >= 1) {
          hpx::cout << (boost::format("New Incumbent Bound: %1%\n") % incumbentNode.getObj());
        }
      }
    }

    Node getIncumbent() const {
      return incumbentNode;
    }
  };

  template<typename Node, typename Bound, typename Cmp, typename Verbose>
  void initialiseIncumbent(Node n, Bound b) {
    auto p = ptr.get();
    auto cmp = static_cast<Incumbent::IncumbentComp<Node, Bound, Cmp, Verbose>*>(p);
    cmp->initialiseIncumbent(n, b);
  }

  template<typename Node, typename Bound, typename Cmp, typename Verbose>
  void updateIncumbent(Node incumbent) {
    auto p = ptr.get();
    auto cmp = static_cast<Incumbent::IncumbentComp<Node, Bound, Cmp, Verbose>*>(p);
    cmp->updateIncumbent(incumbent);
  }

  template<typename Node, typename Bound, typename Cmp, typename Verbose>
  Node getIncumbent() const {
    auto p = ptr.get();
    auto cmp = static_cast<Incumbent::IncumbentComp<Node, Bound, Cmp, Verbose>*>(p);
    return cmp->getIncumbent();
  }

  template<typename Node, typename Bound, typename Cmp, typename Verbose>
  struct UpdateIncumbentAct : hpx::actions::make_action<
    decltype(&Incumbent::updateIncumbent<Node, Bound, Cmp, Verbose>),
    &Incumbent::updateIncumbent<Node, Bound, Cmp, Verbose>,
    UpdateIncumbentAct<Node, Bound, Cmp, Verbose> >::type {};

  template<typename Node, typename Bound, typename Cmp, typename Verbose>
  struct InitialiseIncumbentAct : hpx::actions::make_action<
    decltype(&Incumbent::initialiseIncumbent<Node, Bound, Cmp, Verbose>),
    &Incumbent::initialiseIncumbent<Node, Bound, Cmp, Verbose>,
    InitialiseIncumbentAct<Node, Bound, Cmp, Verbose> >::type {};

  template<typename Node, typename Bound, typename Cmp, typename Verbose>
  struct GetIncumbentAct : hpx::actions::make_action<
    decltype(&Incumbent::getIncumbent<Node, Bound, Cmp, Verbose>),
    &Incumbent::getIncumbent<Node, Bound, Cmp, Verbose>,
    GetIncumbentAct<Node, Bound, Cmp, Verbose> >::type {};
};

}

typedef hpx::components::component<YewPar::Incumbent> incumbent_comp;
HPX_REGISTER_COMPONENT(incumbent_comp);

#endif
