#include <iostream>
#include <unordered_set>

#include <hpx/hpx_init.hpp>

#include "parser.hpp"

#include "skeletons/Seq.hpp"
#include "skeletons/DepthSpawning.hpp"
#include "skeletons/Ordered.hpp"

#define MAX_CITIES  128

struct TSPSol {
  std::vector<unsigned> cities;
  unsigned tourLength;

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & cities;
    ar & tourLength;
  }
};

struct TSPNode {
  TSPSol sol;
  std::unordered_set<unsigned> unvisited;

  unsigned getObj() {
    if (unvisited.empty()) {
      return sol.tourLength;
    } else {
      return INT_MAX;
    }
  }

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & sol;
    ar & unvisited;
  }

};

struct NodeGen : YewPar::NodeGenerator<TSPNode, DistanceMatrix<MAX_CITIES> > {
  unsigned lastCity;

  std::reference_wrapper<const DistanceMatrix<MAX_CITIES> > distances;
  std::reference_wrapper<const TSPNode> parent;

  std::unordered_set<unsigned>::const_iterator unvisitedIter;

  NodeGen(const DistanceMatrix<MAX_CITIES> & distances, const TSPNode & n) :
      distances(std::cref(distances)), parent(std::cref(n)) {
    lastCity = n.sol.cities.back();
    numChildren = n.unvisited.size();
    unvisitedIter = parent.get().unvisited.cbegin();
  }

  TSPNode next() override {
    auto nextCity = *unvisitedIter;
    unvisitedIter++;

    // Not quite right since partial tours don't have a length
    auto newSol = parent.get().sol;
    newSol.cities.push_back(nextCity);
    newSol.tourLength += distances.get()[lastCity][nextCity];

    auto newUnvisited = parent.get().unvisited;
    newUnvisited.erase(nextCity);

    // Link back to the start if we have a complete tour
    if (newUnvisited.empty()) {
      auto start = newSol.cities.front();
      newSol.cities.push_back(start);
      newSol.tourLength += distances.get()[nextCity][start];
    }

    return TSPNode { newSol, newUnvisited };
  }
};

// Very simple MST function, nothing fancy so not the fastest
unsigned mst(const DistanceMatrix<MAX_CITIES> & distances,
             unsigned lastCity,
             std::unordered_set<unsigned> & remCities) {
  std::unordered_map<unsigned, unsigned> weights;
  auto w = 0;
  auto minCity = 0;
  auto minWeight = INT_MAX;

  // Set up initial weights
  for (const auto  & c : remCities) {
    weights[c] = distances[lastCity][c];
  }

  while (!weights.empty()) {
    auto minWeight = INT_MAX;
    for (const auto & c : weights) {
      if (c.second < minWeight) {
        minWeight = c.second;
        minCity = c.first;
      }
    }

    w += minWeight;
    weights.erase(minCity);

    // Update weights
    for (const auto & c : weights) {
      auto dist = distances[minCity][c.first];
      if (dist < c.second) {
        weights[c.first] = dist;
      }
    }
  }

  return w;
}

unsigned boundFn(const DistanceMatrix<MAX_CITIES> & distances, const TSPNode & n) {
  std::unordered_set<unsigned> nodes = n.unvisited;
  nodes.insert(n.sol.cities.front());
  return n.sol.tourLength + mst(distances, n.sol.cities.back(), nodes);
}

typedef func<decltype(&boundFn), &boundFn> upperBound_func;

// TSP helper functions
unsigned calculateTourLength(const DistanceMatrix<MAX_CITIES> & distances,
                             const std::vector<unsigned> & cities) {
  auto l = 0;
  for (auto i = 0; i < cities.size() - 1; ++i) {
    l += distances[cities[i]][cities[i+1]];
  }
  return l;
}

int hpx_main(boost::program_options::variables_map & opts) {
  auto inputFile = opts["input-file"].as<std::string>();

  TSPFromFile inputData;
  try {
    inputData = parseFile(inputFile);
  } catch (SomethingWentWrong & e) {
    std::cerr << e.what() << std::endl;
    hpx::finalize();
  }

  DistanceMatrix<MAX_CITIES> distances;
  if (inputData.type == TSP_TYPE::EUC_2D) {
    distances = buildDistanceMatrixEUC2D<MAX_CITIES>(inputData);
  } else {
    distances = buildDistanceMatrixGEO<MAX_CITIES>(inputData);
  }

  std::vector<unsigned> initialTour {1};
  std::unordered_set<unsigned> unvisited;

  for (auto i = 2; i <= inputData.numNodes; ++i) {
    unvisited.insert(i);
  }

  TSPSol initSol { initialTour, 0};
  TSPNode root { initSol, unvisited };

  auto sol = YewPar::Skeletons::Seq<NodeGen,
                                    YewPar::Skeletons::API::BnB,
                                    YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                    YewPar::Skeletons::API::ObjectiveComparison<std::less<unsigned>>>
             ::search(distances, root);

  std::cout << "Tour: ";
  for (const auto c : sol.sol.cities) {
    std::cout << c << ",";
  }
  std::cout << std::endl;
  std::cout << "Optimal tour length: " << sol.sol.tourLength << "\n";

  return hpx::finalize();
}

int main(int argc, char* argv[]) {
  boost::program_options::options_description
      desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

  desc_commandline.add_options()
      ( "spawn-depth,d",
        boost::program_options::value<std::uint64_t>()->default_value(0),
        "Depth in the tree to spawn until (for parallel skeletons only)"
        )
      ( "input-file,f",
        boost::program_options::value<std::string>(),
        "Input problem"
        )
      ( "skeleton",
        boost::program_options::value<std::string>()->default_value("seq"),
        "Type of skeleton to use: seq, dist"
        );

  return hpx::init(desc_commandline, argc, argv);
}
