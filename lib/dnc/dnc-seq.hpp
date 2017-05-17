#ifndef SKELETONS_DNC_SEQ_HPP
#define SKELETONS_DNC_SEQ_HPP

namespace skeletons { namespace DnC { namespace Seq {

// Trivial : A -> Bool
// Divide : (A -> (A, A)
// Conquer : (A, A) -> A
// F : A -> A
template <typename A,
          typename Divide,
          typename Conquer,
          typename Trivial,
          typename F>
A dnc(const Divide && divide,
      const Conquer && conquer,
      const Trivial && trivial,
      const F && f,
      const A x) {
  if (trivial(x)) {
    return f(x);
  }

  auto p = divide(x);

  auto l = std::get<0>(p);
  auto r = std::get<1>(p);

  auto l_res = dnc(divide, conquer, trivial, f, l);
  auto r_res = dnc(divide, conquer, trivial, f, r);

  return conquer(l_res, r_res);
}

}}}

#endif
