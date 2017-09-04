#ifndef NODESTACK_HPP
#define NODESTACK_HPP

#include <vector>
#include <array>
#include <mutex>
#include <utility>
#include <hpx/lcos/future.hpp>
#include <hpx/lcos/wait_all.hpp>
#include <hpx/util/spinlock.hpp>
#include <hpx/runtime/naming/id_type.hpp>
#include <hpx/util/tuple.hpp>
#include <boost/optional.hpp>

// Simple stack of nodes that supports direct stealing of elements
template <typename Sol, typename GenType, std::size_t N>
class NodeStack {
  using elem_type = std::pair<unsigned, GenType>;

 private:
  hpx::util::spinlock mtx;
  unsigned stackPtr;
  std::array<elem_type, N> stack;
  std::vector<hpx::future<void> > futures;
  unsigned depth;

 public:
  NodeStack() : stackPtr(0), depth(1) {
    futures.reserve(N);
  };

  NodeStack(unsigned depth) : stackPtr(0), depth(depth) {
    futures.reserve(N);
  };

  void push(const GenType & gen) {
    std::lock_guard<hpx::util::spinlock> l(mtx);
    stackPtr++;
    depth++;
    stack[stackPtr] = std::make_pair(gen.numChildren, gen);
  };

  bool empty() {
    std::lock_guard<hpx::util::spinlock> l(mtx);
    return (stackPtr == 0);
  }

  elem_type& getCurrentFrame() {
    std::lock_guard<hpx::util::spinlock> l(mtx);
    return stack[stackPtr];
  }

  void pop() {
    std::lock_guard<hpx::util::spinlock> l(mtx);
    stackPtr--;
    depth--;
  };

  boost::optional<hpx::util::tuple<Sol, unsigned, hpx::naming::id_type> > steal () {
    std::lock_guard<hpx::util::spinlock> l(mtx);

    // Note strictly less than stackPtr required since a thread may already be
    // looking at a reference to stack[stackPtr]
    for (auto stealPos = 1; stealPos < stackPtr; ++stealPos) {
      if (std::get<0>(stack[stackPtr]) > 0) {
        hpx::promise<void> prom;
        auto f   = prom.get_future();
        auto pid = prom.get_id();
        futures.push_back(std::move(f));

        std::get<0>(stack[stackPtr])--;
        return hpx::util::make_tuple(std::get<1>(stack[stackPtr]).next(), depth, pid);
      }
    }
    return {};
  }

  void waitFutures() {
    // We don't want to hold the lock here. Only called once we are done
    // everything else so no steals will succeed and no one else can modify the
    // index so this is safe
    hpx::wait_all(futures);
  }
};

#endif
