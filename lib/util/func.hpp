#ifndef YEWPAR_FUNCVIEW_HPP
#define YEWPAR_FUNCVIEW_HPP

// TODO: Put into YewPar namespace

// Simple wrapping of function pointers to pass as (local only) template parameters
// HPX "actions" should be used if distributed execution is required
template <typename Func, Func F>
struct func;

template <typename Ret, typename ...Args, Ret(*F)(Args...)>
struct func<Ret(*)(Args...), F> {
  using return_type = Ret;
  using func_type = Ret(Args...);

  static Ret invoke(Args... xs) {
    return F(std::forward<Args>(xs)...);
  }

  static auto fn_ptr() {
    return F;
  }
};

static bool null__() { };
typedef func<decltype(&null__), &null__> nullFn__;

#endif
