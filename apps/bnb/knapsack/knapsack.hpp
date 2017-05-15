#ifndef KNAPSACK_HPP
#define KNAPSACK_HPP

#include <vector>
#include <array>
#include <cmath>

#include <hpx/config.hpp>
#include <boost/serialization/access.hpp>

#include <hpx/util/tuple.hpp>

/* A representation of a knapsack current solution */
struct KPSolution {
  friend class boost::serialization::access;

  int numItems;
  int capacity;
  std::vector<int> items;
  int profit;
  int weight;

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & numItems;
    ar & numItems;
    ar & capacity;
    ar & items;
    ar & profit;
    ar & weight;
  }
};

using KPNode = hpx::util::tuple<KPSolution, int, std::vector<int> >;

template <unsigned numItems>
using KPSpace = std::pair< std::array<int, numItems>, std::array<int, numItems> >;

template <unsigned numItems>
std::vector<KPNode> generateChoices(const KPSpace<numItems> & space, const KPNode & n) {
  auto cands = hpx::util::get<2>(n);

  // Get potential choices - those that don't exceed the capacity
  std::vector<int> items;
  std::copy_if(cands.begin(), cands.end(), std::inserter(items, items.end()), [&](const int i) {
      return hpx::util::get<0>(n).weight + std::get<1>(space)[i] <= hpx::util::get<0>(n).capacity;
    });

  // Create new search  nodes for each potential choice
  std::vector< hpx::util::tuple<KPSolution, int, std::vector<int> > > ret;
  for (const auto & i : items) {

    auto newSol = hpx::util::get<0>(n);
    newSol.items.push_back(i);
    newSol.profit += std::get<0>(space)[i];
    newSol.weight += std::get<1>(space)[i];

    auto newCands = cands;
    newCands.erase(std::remove(newCands.begin(), newCands.end(), i), newCands.end());

    ret.push_back(hpx::util::make_tuple(newSol, newSol.profit, newCands));
  }

  return ret;
}

template <unsigned numItems>
int upperBound(const KPSpace<numItems> & space, const KPNode & n) {
  auto sol = hpx::util::get<0>(n);

  float profit = sol.profit;
  auto weight  = sol.weight;

  for (auto i = sol.items.back() + 1; i < sol.numItems; i++) {
    // If there is enough space for a full item we take it all
    if (hpx::util::get<1>(space)[i] + weight > sol.capacity) {
      profit += std::get<0>(space)[i];
      weight += std::get<1>(space)[i];
    } else {
      // Only space for some fraction of the last item
      profit = profit + (sol.capacity - weight) * (std::get<0>(space)[i] / std::get<1>(space)[i]);
      break;
    }
  }

  return std::ceil(profit);
}

#endif
