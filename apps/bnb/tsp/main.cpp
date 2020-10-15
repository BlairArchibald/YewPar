#include <iostream>
#include <set>
#include <chrono>
#include <bitset>

#include <hpx/hpx_init.hpp>
#include <hpx/include/iostreams.hpp>
#include <hpx/serialization/bitset.hpp>

#include "parser.hpp"
#include "YewPar.hpp"

#include "skeletons/Seq.hpp"
#include "skeletons/DepthBounded.hpp"
#include "skeletons/Ordered.hpp"
#include "skeletons/Budget.hpp"
#include "skeletons/StackStealing.hpp"

#define MAX_CITIES  64

struct TSPSol {
  std::vector<unsigned> cities;
  unsigned tourLength;

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & cities;
    ar & tourLength;
  }
};

struct TSPSpace {
  DistanceMatrix<MAX_CITIES> distances;
  unsigned numCities;

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & distances;
    ar & numCities;
  }
};

struct TSPNode {
  TSPSol sol;
  std::bitset<MAX_CITIES> unvisited;

  unsigned getObj() const {
    if (unvisited.none()) {
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


template <std::size_t words_>
static int next_set(const std::bitset<words_> & bs, unsigned max, unsigned last_set) {
  for (auto i = last_set + 1; i <= max; ++i) {
    if (bs.test(i)) {
      return i;
    }
  }
  return -1; // Error
}

struct NodeGen : YewPar::NodeGenerator<TSPNode, TSPSpace> {
  unsigned lastCity;

  std::reference_wrapper<const TSPSpace> space;
  std::reference_wrapper<const TSPNode> parent;

  unsigned nextToVisit;

  NodeGen(const TSPSpace & space, const TSPNode & n) :
      space(std::cref(space)), parent(std::cref(n)) {
    lastCity = n.sol.cities.back();
    numChildren = n.unvisited.count();
    const auto & bs = parent.get().unvisited;
    nextToVisit = next_set<MAX_CITIES>(bs, this->space.get().numCities, 0);
  }

  TSPNode next() override {
    auto nextCity = nextToVisit;
    nextToVisit = next_set<MAX_CITIES>(parent.get().unvisited, space.get().numCities, nextToVisit);

    // Not quite right since partial tours don't have a length
    auto newSol = parent.get().sol;
    newSol.cities.push_back(nextCity);
    newSol.tourLength += space.get().distances[lastCity][nextCity];

    auto newUnvisited = parent.get().unvisited;
    newUnvisited.reset(nextCity);

    // Link back to the start if we have a complete tour
    if (newUnvisited.none()) {
      auto start = newSol.cities.front();
      newSol.cities.push_back(start);
      newSol.tourLength += space.get().distances[nextCity][start];
    }

    return TSPNode { newSol, newUnvisited };
  }
};

// Very simple MST function, nothing fancy so not the fastest
unsigned mst(const TSPSpace & space,
             unsigned lastCity,
             std::bitset<MAX_CITIES> & remCities) {
  std::array<unsigned, MAX_CITIES> weights;

  auto w = 0;
  auto minCity = 0;
  auto minWeight = INT_MAX;

  // Set up initial weights
  for (auto i = 1; i <= space.numCities; ++i) {
    if (remCities.test(i)) {
      weights[i] = space.distances[lastCity][i];
    }
  }

  while (!remCities.none()) {
    auto minWeight = INT_MAX;
    for (auto i = 1; i <= space.numCities; ++i) {
      if (remCities.test(i)) {
        if (weights[i]< minWeight) {
          minWeight = weights[i];
          minCity = i;
        }
      }
    }

    w += minWeight;
    remCities.reset(minCity);

    // Update weights
      for (auto i = 1; i <= space.numCities; ++i) {
        if (remCities.test(i)) {
          auto dist = space.distances[minCity][i];
          if (dist < weights[i]) {
            weights[i] = dist;
          }
        }
      }
  }

  return w;
}

unsigned boundFn(const TSPSpace & space, const TSPNode & n) {
  std::bitset<MAX_CITIES> nodes = n.unvisited;
  nodes.set(n.sol.cities.front());
  return n.sol.tourLength + mst(space, n.sol.cities.back(), nodes);
}

typedef func<decltype(&boundFn), &boundFn> upperBound_func;

unsigned greedyNN(const DistanceMatrix<MAX_CITIES> & distances,
                  const std::vector<unsigned> & cities,
                  const unsigned startingCity) {
  unsigned dist = 0;

  std::set<unsigned> rem;
  for (const auto & c : cities) {
    rem.insert(c);
  }

  rem.erase(startingCity);
  auto curCity = startingCity;

  while (!rem.empty()) {
    auto nextCity = *(std::min_element(rem.begin(), rem.end(),
                                       [curCity, distances](const unsigned & x, const unsigned & y){
                                         return distances[curCity][x] < distances[curCity][y];
                                       }));
    dist += distances[curCity][nextCity];
    rem.erase(nextCity);
    curCity = nextCity;
  };

  // Add loop back to start
  dist += distances[curCity][startingCity];

  return dist;
}


// TSP helper functions
unsigned calculateTourLength(const DistanceMatrix<MAX_CITIES> & distances,
                             const std::vector<unsigned> & cities) {
  auto l = 0;
  for (auto i = 0; i < cities.size() - 1; ++i) {
    l += distances[cities[i]][cities[i+1]];
  }
  return l;
}

int hpx_main(hpx::program_options::variables_map & opts) {
  auto inputFile = opts["input-file"].as<std::string>();

  TSPFromFile inputData;
  try {
    inputData = parseFile(inputFile);
  } catch (SomethingWentWrong & e) {
    std::cerr << e.what() << hpx::endl;
    hpx::finalize();
  }

  DistanceMatrix<MAX_CITIES> distances;
  if (inputData.type == TSP_TYPE::EUC_2D) {
    distances = buildDistanceMatrixEUC2D<MAX_CITIES>(inputData);
  } else {
    distances = buildDistanceMatrixGEO<MAX_CITIES>(inputData);
  }

  std::vector<unsigned> initialTour {1};
  std::bitset<MAX_CITIES> unvisited;

  for (auto i = 2; i <= inputData.numNodes; ++i) {
    unvisited.set(i);
  }

  auto start_time = std::chrono::steady_clock::now();

  TSPSpace space { distances , inputData.numNodes };
  TSPSol initSol { initialTour, 0};
  TSPNode root { initSol, unvisited };

  auto skeletonType = opts["skeleton"].as<std::string>();
  auto spawnDepth = opts["spawn-depth"].as<unsigned>();
  auto sol = root;

  // Init the bound to a greedy nearest neighbour search
  std::vector<unsigned> allCities(inputData.numNodes + 1);
  std::iota(allCities.begin(), allCities.end(), 1);
  YewPar::Skeletons::API::Params<unsigned> searchParameters;
  searchParameters.initialBound = greedyNN(distances, allCities, 1);

  if (skeletonType == "seq") {

    sol = YewPar::Skeletons::Seq<NodeGen,
                                 YewPar::Skeletons::API::Optimisation,
                                 YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                 YewPar::Skeletons::API::ObjectiveComparison<std::less<unsigned>>>
        ::search(space, root, searchParameters);
  } else if (skeletonType == "depthbounded") {
    searchParameters.spawnDepth = spawnDepth;
    sol = YewPar::Skeletons::DepthBounded<NodeGen,
                                         YewPar::Skeletons::API::Optimisation,
                                         YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                         YewPar::Skeletons::API::ObjectiveComparison<std::less<unsigned>>>
               ::search(space, root, searchParameters);
  } else if (skeletonType == "ordered") {
    searchParameters.spawnDepth = spawnDepth;
    if (opts.count("discrepancyOrder")) {
      sol = YewPar::Skeletons::Ordered<NodeGen,
                                      YewPar::Skeletons::API::Optimisation,
                                      YewPar::Skeletons::API::DiscrepancySearch,
                                      YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                      YewPar::Skeletons::API::ObjectiveComparison<std::less<unsigned>>>
                ::search(space, root, searchParameters);
    } else {
      sol = YewPar::Skeletons::Ordered<NodeGen,
                                      YewPar::Skeletons::API::Optimisation,
                                      YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                      YewPar::Skeletons::API::ObjectiveComparison<std::less<unsigned>>>
                ::search(space, root, searchParameters);
    }
  } else if (skeletonType == "budget") {
    searchParameters.backtrackBudget = opts["backtrack-budget"].as<unsigned>();
    sol = YewPar::Skeletons::Budget<NodeGen,
                                    YewPar::Skeletons::API::Optimisation,
                                    YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                    YewPar::Skeletons::API::ObjectiveComparison<std::less<unsigned>>>
        ::search(space, root, searchParameters);
  } else if (skeletonType == "stacksteal") {
    searchParameters.stealAll = static_cast<bool>(opts.count("chunked"));
    sol = YewPar::Skeletons::StackStealing<NodeGen,
                                           YewPar::Skeletons::API::Optimisation,
                                           YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                           YewPar::Skeletons::API::ObjectiveComparison<std::less<unsigned>>>
        ::search(space, root, searchParameters);
  } else {
    hpx::cout << "Invalid skeleton type\n";
    return hpx::finalize();
  }

  auto overall_time = std::chrono::duration_cast<std::chrono::milliseconds>
                      (std::chrono::steady_clock::now() - start_time);

  hpx::cout << "Tour: ";
  for (const auto c : sol.sol.cities) {
    hpx::cout << c << ",";
  }
  hpx::cout << hpx::endl;
  hpx::cout << "Optimal tour length: " << sol.sol.tourLength << "\n";

  hpx::cout << "cpu = " << overall_time.count() << hpx::endl;

  return hpx::finalize();
}

int main(int argc, char* argv[]) {
  hpx::program_options::options_description
      desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

  desc_commandline.add_options()
      ( "skeleton",
        hpx::program_options::value<std::string>()->default_value("seq"),
        "Which skeleton to use: seq, depthbound, stacksteal, budget, or ordered"
        )
      ( "input-file,f",
        hpx::program_options::value<std::string>()->required(),
        "Input problem"
        )
      ( "backtrack-budget,b",
        hpx::program_options::value<unsigned>()->default_value(500),
        "Number of backtracks before spawning work"
        )
       ("discrepancyOrder", "Use discrepancy order for the ordered skeleton")
       ("chunked", "Use chunking with stack stealing")
       ( "spawn-depth,d",
        hpx::program_options::value<unsigned>()->default_value(0),
        "Depth in the tree to spawn until (for parallel skeletons only)"
        );

  YewPar::registerPerformanceCounters();

  return hpx::init(desc_commandline, argc, argv);
}
