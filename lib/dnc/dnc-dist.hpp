#ifndef SKELETONS_DNC_DIST_HPP
#define SKELETONS_DNC_DIST_HPP

#include <hpx/lcos/broadcast.hpp>

#include "workstealing/scheduler.hpp"
#include "workstealing/workqueue.hpp"

namespace skeletons { namespace DnC { namespace Dist {

// Forward Decls
template <typename A,
          typename Divide,
          typename Conquer,
          typename Trivial,
          typename F,
          typename RecAct>
A dncRec(const A x);

// Helper function that tells the scheduler we need more work if we are going to
// block
template <typename T>
T signalGet(hpx::future<T> & fut) {
  if (fut.is_ready()) {
    return fut.get();
  } else {
    workstealing::tasks_required_sem.signal(1);
    return fut.get();
  }
}

template <typename A,
          typename Divide,
          typename Conquer,
          typename Trivial,
          typename F,
          typename RecAct>
A dnc(const A x) {
  // Start work stealing schedulers
  std::vector<hpx::naming::id_type> workqueues;
  for (auto const& loc : hpx::find_all_localities()) {
    workqueues.push_back(hpx::new_<workstealing::workqueue>(loc).get());
  }
  hpx::wait_all(hpx::lcos::broadcast<startScheduler_action>(hpx::find_all_localities(), workqueues));

  // Do computation
  auto ret = dncRec<A, Divide, Conquer, Trivial, F, RecAct>(x);

  // Stop schedulers
  hpx::wait_all(hpx::lcos::broadcast<stopScheduler_action>(hpx::find_all_localities()));

  return ret;
}

template <typename A,
          typename Divide,
          typename Conquer,
          typename Trivial,
          typename F,
          typename RecAct>
A dncRec(const A x) {
  Trivial t;
  if (t(hpx::find_here(), x)) {
    F f;
    return f(hpx::find_here(), x);
  }

  Divide d;
  auto p = d(hpx::find_here(), x);

  auto l = std::get<0>(p);
  auto r = std::get<1>(p);

  hpx::lcos::promise<A> prom;
  auto pfut = prom.get_future();
  auto pid  = prom.get_id();

  RecAct a;
  hpx::util::function<void(hpx::naming::id_type)> task = hpx::util::bind(a, hpx::util::placeholders::_1, l, pid);
  hpx::apply<workstealing::workqueue::addWork_action>(workstealing::local_workqueue, task);

  auto r_res = dncRec<A, Divide, Conquer, Trivial, F, RecAct>(r);

  auto l_res = signalGet(pfut);

  Conquer c;
  return c(hpx::find_here(), l_res, r_res);
}

template <typename A,
          typename Divide,
          typename Conquer,
          typename Trivial,
          typename F,
          typename RecAct>
void dncTask(const A x, hpx::naming::id_type promise) {
  auto res = dncRec<A, Divide, Conquer, Trivial, F, RecAct>(x);
  hpx::apply<typename hpx::lcos::base_lco_with_value<A>::set_value_action>(promise, std::move(res));
}

}}}

#endif
