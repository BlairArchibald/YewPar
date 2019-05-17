#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <regex>
#include <exception>
#include <chrono>

#include <hpx/hpx_init.hpp>
#include <hpx/include/iostreams.hpp>

#include "knapsack.hpp"

#include "YewPar.hpp"
#include "util/func.hpp"
#include "skeletons/Seq.hpp"
#include "skeletons/DepthSpawning.hpp"
#include "skeletons/Ordered.hpp"
#include "skeletons/Budget.hpp"
#include "skeletons/StackStealing.hpp"

#ifndef NUMITEMS
#define NUMITEMS 50
#endif

typedef func<decltype(&upperBound<NUMITEMS>), &upperBound<NUMITEMS> > bnd_func;

struct knapsackData {
  int capacity = 0;
  int expectedResult = 0;
  std::vector<std::pair<int,int> > items;
};

knapsackData read_knapsack(const std::string & filename) {
  std::ifstream infile { filename };
  if (!infile) {
    throw "Unable to open file";
  }

  knapsackData kp;
  std::string line;

  // Capacity
  if (std::getline(infile, line)) {
    kp.capacity = std::stoi(line);
  } else {
    throw "Could not read capacity from file";
  }

  // Expected Result
  if (std::getline(infile, line)) {
    kp.expectedResult = std::stoi(line);
  } else {
    throw "Could not read expected result from file";
  }

  // Read the items
  while (std::getline(infile, line)) {
    const std::regex item { R"((\d+)\s+(\d+)\s*)" };
    std::smatch match;
    if (regex_match(line, match, item)) {
      kp.items.push_back(std::make_pair(std::stoi(match.str(1)), std::stoi(match.str(2))));
    }
  }

  return kp;
}

int hpx_main(boost::program_options::variables_map & opts) {


  knapsackData problem;
  auto inputFile = opts["input-file"].as<std::string>();
  try {
    problem = read_knapsack(inputFile);
  } catch (std::string e) {
    hpx::cout << "Error in file parsing" << hpx::endl;
    hpx::finalize();
    return EXIT_FAILURE;
  }

  // Ensure profit density ordering
  std::sort(problem.items.begin(), problem.items.end(),
            [](const std::pair<int, int> & x, const std::pair<int, int> & y) {
              double p1,w1,p2,w2;
              std::tie(p1, w1) = x;
              std::tie(p2, w2) = y;
              if (p1 / w1 == p2 / w2) {
                if (p1 == p2) {
                  return w1 < w2;
                } else {
                  return p1 > p2;
                }
              } else {
                return p1 / w1 > p2 / w2;
              }
            });

  // Pack the problem into a more efficient format
  std::array<int, NUMITEMS> profits;
  std::array<int, NUMITEMS> weights;
  int i = 0;
  for (auto const & item : problem.items) {
    int profit = std::get<0>(item);
    int weight = std::get<1>(item);

    profits[i] = profit;
    weights[i] = weight;

    i++;
  }

  int numItems = problem.items.size();

  // Check profit density ordering (required for bounding to work correctly)
  for (int i = 0; i < numItems - 1; i++) {
    auto x = (double) profits[i + 1] / (double) weights[i + 1];
    auto y = (double) profits[i] / (double) weights[i];
    if (x > y) {
      hpx::cout << "Input not in profit density ordering" << hpx::endl;
      hpx::finalize();
      return EXIT_FAILURE;
    }
  }

  auto start_time = std::chrono::steady_clock::now();

  KPSpace<NUMITEMS> space {profits, weights, numItems, problem.capacity};
  KPSolution initSol = {std::vector<int>(), 0, 0};

  std::vector<int> initRem;
  for (int i = 0; i < numItems; i++) {
    initRem.push_back(i);
  }
  KPNode root {initSol, initRem};

  auto sol = root;
  auto skeletonType = opts["skeleton"].as<std::string>();
  if (skeletonType == "seq") {
    sol = YewPar::Skeletons::Seq<GenNode<NUMITEMS>,
                                 YewPar::Skeletons::API::Optimisation,
                                 YewPar::Skeletons::API::PruneLevel,
                                 YewPar::Skeletons::API::BoundFunction<bnd_func> >
          ::search(space, root);
  } else if (skeletonType == "depthbounded") {
    auto spawnDepth = opts["spawn-depth"].as<unsigned>();
    YewPar::Skeletons::API::Params<int> searchParameters;
    searchParameters.spawnDepth = spawnDepth;
    sol = YewPar::Skeletons::DepthSpawns<GenNode<NUMITEMS>,
                                         YewPar::Skeletons::API::Optimisation,
                                         YewPar::Skeletons::API::PruneLevel,
                                         YewPar::Skeletons::API::BoundFunction<bnd_func> >
          ::search(space, root, searchParameters);
  } else if (skeletonType == "ordered") {
    auto spawnDepth = opts["spawn-depth"].as<unsigned>();
    YewPar::Skeletons::API::Params<int> searchParameters;
    searchParameters.spawnDepth = spawnDepth;
    sol = YewPar::Skeletons::Ordered<GenNode<NUMITEMS>,
                                     YewPar::Skeletons::API::Optimisation,
                                     YewPar::Skeletons::API::PruneLevel,
                                     YewPar::Skeletons::API::BoundFunction<bnd_func> >
          ::search(space, root, searchParameters);
  } else if (skeletonType == "budget") {
    YewPar::Skeletons::API::Params<int> searchParameters;
    searchParameters.backtrackBudget = opts["backtrack-budget"].as<unsigned>();
    sol = YewPar::Skeletons::Budget<GenNode<NUMITEMS>,
                                    YewPar::Skeletons::API::Optimisation,
                                    YewPar::Skeletons::API::PruneLevel,
                                    YewPar::Skeletons::API::BoundFunction<bnd_func> >
        ::search(space, root, searchParameters);
  } else if (skeletonType == "stacksteal") {
    YewPar::Skeletons::API::Params<int> searchParameters;
    searchParameters.stealAll = static_cast<bool>(opts.count("chunked"));
    sol = YewPar::Skeletons::StackStealing<GenNode<NUMITEMS>,
                                           YewPar::Skeletons::API::Optimisation,
                                           YewPar::Skeletons::API::PruneLevel,
                                           YewPar::Skeletons::API::BoundFunction<bnd_func> >
        ::search(space, root, searchParameters);
  } else {
    hpx::cout << "Invalid skeleton type\n";
    hpx::finalize();
    return EXIT_FAILURE;
  }

  auto overall_time = std::chrono::duration_cast<std::chrono::milliseconds>
                      (std::chrono::steady_clock::now() - start_time);

  auto finalSol = sol.sol;
  hpx::cout << "Final Profit: " << finalSol.profit << hpx::endl;
  hpx::cout << "Final Weight: " << finalSol.weight << hpx::endl;
  hpx::cout << "Expected Result: " << std::boolalpha << (finalSol.profit == problem.expectedResult) << hpx::endl;
  hpx::cout << "Items: ";
  for (auto const & i : finalSol.items) {
    hpx::cout << i << " ";
  }
  hpx::cout << hpx::endl;

  hpx::cout << "cpu = " << overall_time.count() << hpx::endl;

  return hpx::finalize();
}

int main(int argc, char* argv[]) {
  boost::program_options::options_description
    desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

  desc_commandline.add_options()
    ( "skeleton",
      boost::program_options::value<std::string>()->default_value("seq"),
      "Which skeleton to use: seq, depthbound, stacksteal, budget, or ordered"
    )
    ( "input-file,f",
      boost::program_options::value<std::string>()->required(),
      "Input problem"
    )
    ( "backtrack-budget,b",
      boost::program_options::value<unsigned>()->default_value(500),
      "Number of backtracks before spawning work"
    )
    ("chunked", "Use chunking with stack stealing")
    ( "spawn-depth,d",
      boost::program_options::value<unsigned>()->default_value(0),
      "Depth in the tree to spawn until (for parallel skeletons only)"
    );

  YewPar::registerPerformanceCounters();

  return hpx::init(desc_commandline, argc, argv);
}
