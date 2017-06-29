#ifndef POSITION_INDEX_HPP
#define POSITION_INDEX_HPP

#include <vector>
#include <mutex>
#include <hpx/hpx.hpp>
#include <hpx/util/spinlock.hpp>
#include <boost/serialization/access.hpp>

class positionIndex {
private:
  unsigned depth;
  std::vector<unsigned> path;
  std::vector<int> children;
  std::vector<int> nextIndex;
  std::map<unsigned, std::vector<hpx::future<void> > > futures;
  hpx::util::spinlock mtx;

public:

  positionIndex() : depth(0) {
    path.reserve(30);
    children.reserve(30);
    nextIndex.reserve(30);

    path.push_back(0);
    children.push_back(-1);
    nextIndex.push_back(-1);
  }

  positionIndex(std::vector<unsigned> path) : depth(path.size()) {
    path = path;

    nextIndex.reserve(30);
    children.reserve(30);
    for (auto i = 0; i < path.size(); i++) {
      children.push_back(0);
      nextIndex.push_back(-1);
    }
  }

  std::vector<unsigned> getPath() {
    std::lock_guard<hpx::util::spinlock> l(mtx);
    return path;
  }

  void setNumChildren(int numChildren) {
    std::lock_guard<hpx::util::spinlock> l(mtx);
    children[depth]  = numChildren;
    if (numChildren > 0) {
      nextIndex[depth] = 0;
    }
  }

  void pruneLevel() {
    std::lock_guard<hpx::util::spinlock> l(mtx);
    children[depth] = 0;
  }

  // Return the "next" node to search, accounting for any steals
  // Essentially gives ownership of this node to the caller
  // Return -1 if there is no next position
  int getNextPosition() {
    std::lock_guard<hpx::util::spinlock> l(mtx);
    if (children[depth] == 0) {
      return -1;
    } else {
      children[depth]--; // We have "taken" this node
      return ++nextIndex[depth]; //Does this account for stolen futures. I think so.
    }
  }

  // TODO: Does this need to be thread safe since it should only be called once
  // there are no positions left? (we don't really want to block with the lock...)
  void waitFutures() {
    hpx::wait_all(futures[depth]);
  }

  void preExpand(unsigned i) {
    std::lock_guard<hpx::util::spinlock> l(mtx);
    path.push_back(i);
    children.push_back(-1);
    nextIndex.push_back(-1);
    depth++;
  }

  void postExpand() {
    std::lock_guard<hpx::util::spinlock> l(mtx);
    depth--;
    path.pop_back();
    children.pop_back();
    nextIndex.pop_back();
  }

  std::pair<std::vector<unsigned>, hpx::naming::id_type> steal() {
    std::lock_guard<hpx::util::spinlock> l(mtx);

    // Find the highest depth that still has work
    std::vector<unsigned> res;

    int highest = -1;
    for (auto i = 0; i <= depth; ++i) {
      if (children[i] > 0) {
        highest = i;
        break;
      }
    }

    if (highest == -1) {
      return std::make_pair(res, hpx::find_here()); //Nothing to steal
    } else {
      children[highest]--;

      // Calc the path
      for (auto i = 0; i <= highest; ++i) {
        res.push_back(path[i]);
      }
      auto idx = ++nextIndex[highest];
      res.push_back(idx);

      hpx::promise<void> prom;
      auto f = prom.get_future();
      auto pid = prom.get_id();

      futures[highest].push_back(std::move(f));

      return std::make_pair(res, pid);
    }
  }

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {}
};

#endif
