#ifndef POSITION_INDEX_HPP
#define POSITION_INDEX_HPP

#include <vector>
#include <hpx/lcos/future.hpp>
#include <hpx/util/tuple.hpp>
#include <boost/optional.hpp>

class positionIndex {
 private:
  unsigned depth;
  std::vector<unsigned> path;
  std::vector<int> children;
  std::vector<int> nextIndex;
  std::vector<hpx::future<void> > futures;

 public:

  positionIndex() : depth(0) {
    path.reserve(30);
    children.reserve(30);
    nextIndex.reserve(30);
    futures.reserve(30);

    path.push_back(0);
    children.push_back(-1);
    nextIndex.push_back(-1);
  }

  positionIndex(std::vector<unsigned> path) : path(path), depth(path.size() - 1) {
    path.reserve(30);
    nextIndex.reserve(30);
    children.reserve(30);
    futures.reserve(30);

    // Root element only
    if (path.size() == 1) {
      children.push_back(-1);
      nextIndex.push_back(-1);
    } else{
      for (auto i = 0; i < depth; i++) {
        children.push_back(0);
        nextIndex.push_back(-1);
      }
      // Where we start from - Do I need this? probably
      children.push_back(-1);
      nextIndex.push_back(-1);
    }
  }

  std::vector<unsigned> getPath() {
    return path;
  }

  void setNumChildren(int numChildren) {
    children[depth]  = numChildren;
    if (numChildren > 0) {
      nextIndex[depth] = 0;
    }
  }

  void pruneLevel() {
    children[depth] = 0;
  }

  // Return the "next" node to search, accounting for any steals
  // Essentially gives ownership of this node to the caller
  // Return -1 if there is no next position
  int getNextPosition() {
    if (children[depth] == 0) {
      return -1;
    } else {
      children[depth]--; // We have "taken" this node
      auto idx = nextIndex[depth];
      nextIndex[depth]++;
      return idx;
    }
  }

  void preExpand(unsigned i) {
    path.push_back(i);
    children.push_back(-1);
    nextIndex.push_back(-1);
    depth++;
  }

  void postExpand() {
    depth--;
    path.pop_back();
    children.pop_back();
    nextIndex.pop_back();
  }

  boost::optional<hpx::util::tuple<std::vector<unsigned>, int, hpx::naming::id_type> > steal() {
    // Find the highest depth that still has work
    int highest = -1;
    for (auto i = 0; i <= depth; ++i) {
      if (children[i] > 0) {
        highest = i;
        break;
      }
    }

    if (highest == -1) {
      return {};
    }

    // Take this child
    children[highest]--;

    // Calc the path
    std::vector<unsigned> res;
    for (auto i = 0; i <= highest; ++i) {
      res.push_back(path[i]);
    }

    auto idx = nextIndex[highest];
    nextIndex[highest]++;
    res.push_back(idx);

    hpx::promise<void> prom;
    auto f = prom.get_future();
    auto pid = prom.get_id();

    futures.push_back(std::move(f));

    // FIXME: Should path size be returned here?
    int stealDepth = res.size();
    auto stealRes = hpx::util::make_tuple(std::move(res), stealDepth, pid);
    return stealRes;
  }

  void waitFutures() {
    hpx::wait_all(futures);
  }
};

#endif
