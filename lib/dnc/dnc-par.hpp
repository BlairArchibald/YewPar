#ifndef SKELETONS_DNC_PAR_HPP
#define SKELETONS_DNC_PAR_HPP

namespace skeletons { namespace DnC { namespace Par {

// Trivial : A -> Bool
// Divide : (A -> (A, A)
// Conquer : (A, A) -> A
// F : A -> A
template <typename A,
          typename Divide,
          typename Conquer,
          typename Trivial,
          typename F,
          typename RecAct>
A dnc(const A x) {
  Trivial t;
  if (t(hpx::find_here(), x)) {
    F f;
    return f(hpx::find_here(), x);
  }

  Divide d;
  auto p = d(hpx::find_here(), x);

  auto l = std::get<0>(p);
  auto r = std::get<1>(p);

  auto fut = hpx::async<RecAct>(hpx::find_here(), l);
  auto r_res = dnc<A, Divide, Conquer, Trivial, F, RecAct>(r);

  auto l_res = fut.get();

  Conquer c;
  return c(hpx::find_here(), l_res, r_res);
}

}}}

#endif
