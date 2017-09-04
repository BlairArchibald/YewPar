#ifndef YEWPAR_FUNCVIEW_HPP
#define YEWPAR_FUNCVIEW_HPP

// Simple wrapping of function pointers to pass as (local only) template parameters
// HPX "actions" should be used if distributed execution is required
template <typename Func, Func F>
struct func;

template <typename Ret, typename ...Args, Ret(*F)(Args...)>
struct func<Ret(*)(Args...), F> {
  using return_type = Ret;
  using func_type = Ret(Args...);

  static constexpr auto fn_ptr = F;

  static Ret invoke(Args... xs) {
    return F(std::forward<Args>(xs)...);
  }
};

#endif
