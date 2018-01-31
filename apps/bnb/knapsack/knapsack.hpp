#ifndef KNAPSACK_HPP
#define KNAPSACK_HPP

#include <vector>
#include <array>
#include <cmath>

#include <hpx/config.hpp>
#include <boost/serialization/access.hpp>

#include <hpx/util/tuple.hpp>

#include "util/NodeGenerator.hpp"

/* A representation of a knapsack current solution */
struct KPSolution {
  friend class boost::serialization::access;

  int numItems; // FIXME: should really be in the space not the sol?
  int capacity; // FIXME: should really be in the space not the sol?
  std::vector<int> items;
  int profit;
  int weight;

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & capacity;
    ar & items;
    ar & profit;
    ar & weight;
  }
};

template <unsigned numItems>
using KPSpace = std::pair< std::array<int, numItems>, std::array<int, numItems> >;

struct KPNode {
  friend class boost::serialization::access;

  KPSolution sol;
  std::vector<int> rem;

  int getObj() const {
    return sol.profit;
  }

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & sol;
    ar & rem;
  }
};

template <unsigned numItems>
struct GenNode : YewPar::NodeGenerator<KPNode, KPSpace<numItems> > {
  std::vector<int> items;
  int pos;

  const KPSpace<numItems> & space;
  const KPNode & n;

  GenNode (const KPSpace<numItems> & space, const KPNode & n) :
      pos(0), space(space), n(n) {

    auto cands = n.rem;
    std::copy_if(cands.begin(), cands.end(), std::inserter(items, items.end()), [&](const int i) {
        return n.sol.weight + std::get<1>(space)[i] <= n.sol.capacity;
      });

    this->numChildren = items.size();
  }

  KPNode next() override {
    auto i = items[pos];

    auto newSol = n.sol;
    newSol.items.push_back(i);
    newSol.profit += std::get<0>(space)[i];
    newSol.weight += std::get<1>(space)[i];

    auto newCands = n.rem;
    newCands.erase(std::remove(newCands.begin(), newCands.end(), i), newCands.end());

    pos++;
    return {newSol, newCands};
  }
};

template <unsigned numItems>
int upperBound(const KPSpace<numItems> & space, const KPNode & n) {
  auto sol = n.sol;

  double profit = sol.profit;
  auto weight  = sol.weight;

  for (auto i = sol.items.back() + 1; i < sol.numItems; i++) {
    // If there is enough space for a full item we take it all
    if (hpx::util::get<1>(space)[i] + weight <= sol.capacity) {
      profit += std::get<0>(space)[i];
      weight += std::get<1>(space)[i];
    } else {
      // Only space for some fraction of the last item
      profit = profit + (sol.capacity - weight) * ((double) std::get<0>(space)[i] / (double) std::get<1>(space)[i]);
      break;
    }
  }

  return std::ceil(profit);
}

#endif
