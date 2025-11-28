#ifndef KNAPSACK_HPP
#define KNAPSACK_HPP

#include <vector>
#include <array>
#include <cmath>

#include <hpx/config.hpp>
#include <boost/serialization/access.hpp>

#include <hpx/datastructures/tuple.hpp>

#include "util/NodeGenerator.hpp"

/* A representation of a knapsack current solution */
struct KPSolution {
  std::vector<int> items;
  int profit;
  int weight;

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & items;
    ar & profit;
    ar & weight;
  }
};

template <unsigned N>
struct KPSpace {
  std::array<int, N> profits;
  std::array<int, N> weights;
  int numItems;
  int capacity;

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & profits;
    ar & weights;
    ar & numItems;
    ar & capacity;
  }
};

struct KPNode {
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

  const KPSpace<numItems> &space;
  const KPNode &n;

  GenNode (const KPSpace<numItems> & space, const KPNode & n) :
      pos(0), space(space), n(n) {
    this->numChildren = n.rem.size();
  }

  KPNode next() override {
    auto i = n.rem[pos];
    auto newSol = n.sol;
    newSol.items.push_back(i);
    newSol.profit += space.profits[i];
    newSol.weight += space.weights[i];

    ++pos;

    std::vector<int> newRem;
    std::copy_if(n.rem.begin() + pos, n.rem.end(), std::back_inserter(newRem),
                 [&](const int i) {
                   return newSol.weight + space.weights[i] <= space.capacity;
                 });

    return { newSol, std::move(newRem) };
  }
};

template <unsigned numItems>
int upperBound(const KPSpace<numItems> & space, const KPNode & n) {
  auto sol = n.sol;

  double profit = sol.profit;
  auto weight  = sol.weight;

  for (auto i = sol.items.back() + 1; i < space.numItems; i++) {
    // If there is enough space for a full item we take it all
    if (space.weights[i] + weight <= space.capacity) {
      profit += space.profits[i];
      weight += space.weights[i];
    } else {
      // Only space for some fraction of the last item
      profit = profit + (space.capacity - weight) * ((double) space.profits[i] / (double) space.weights[i]);
      break;
    }
  }

  return std::ceil(profit);
}

#endif
