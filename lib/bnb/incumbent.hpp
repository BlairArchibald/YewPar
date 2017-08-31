#ifndef BNB_INCUMBENT_HPP
#define BNB_INCUMBENT_HPP

#include <hpx/hpx.hpp>
#include <hpx/include/components.hpp>

#include <hpx/util/tuple.hpp>

namespace bounds
{
  // FIXME: We probably don't need to store the candidate search space
  template<typename Sol, typename Bnd, typename Cands>
  class Incumbent : public hpx::components::locking_hook<
    hpx::components::component_base<Incumbent<Sol, Bnd, Cands> >
    >
    {
    private:
      hpx::util::tuple<Sol, Bnd, Cands> incumbent_;
    public:
      void updateIncumbent(hpx::util::tuple<Sol, Bnd, Cands> incumbent) {
        if (hpx::util::get<1>(incumbent) > hpx::util::get<1>(incumbent_)) {
        incumbent_ = incumbent;
        }
      }
      HPX_DEFINE_COMPONENT_ACTION(Incumbent, updateIncumbent);

      hpx::util::tuple<Sol, Bnd, Cands> getIncumbent() const {
        return incumbent_;
      }
      HPX_DEFINE_COMPONENT_ACTION(Incumbent, getIncumbent);
    };
}

#define REGISTER_INCUMBENT(sol,bnd,cand)                                                         \
  typedef bounds::Incumbent<sol, bnd, cand > __incumbent_type_;                                  \
  HPX_REGISTER_ACTION(                                                                           \
    __incumbent_type_::updateIncumbent_action,                                                   \
    BOOST_PP_CAT(__incumbent_update_action_, BOOST_PP_CAT(sol, BOOST_PP_CAT(bnd, cand))));       \
                                                                                                 \
  HPX_REGISTER_ACTION(                                                                           \
    __incumbent_type_::getIncumbent_action,                                                      \
    BOOST_PP_CAT(__incumbent_get_action_, BOOST_PP_CAT(sol, BOOST_PP_CAT(bnd, cand))));          \
                                                                                                 \
  typedef ::hpx::components::component<__incumbent_type_ >                                       \
    BOOST_PP_CAT(__incumbent_, BOOST_PP_CAT(sol, BOOST_PP_CAT(bnd, cand)));                      \
  HPX_REGISTER_COMPONENT(BOOST_PP_CAT(__incumbent_, BOOST_PP_CAT(sol, BOOST_PP_CAT(bnd, cand)))) \

#endif
