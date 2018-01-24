#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <regex>
#include <exception>

#include <hpx/hpx_init.hpp>

#include "knapsack.hpp"

#include "util/func.hpp"
#include "skeletons/Seq.hpp"
#include "skeletons/DepthSpawning.hpp"
#include "skeletons/Ordered.hpp"

#ifndef NUMITEMS
#define NUMITEMS 50
#endif

typedef func<decltype(&upperBound<NUMITEMS>), &upperBound<NUMITEMS> > bnd_func;
REGISTER_INCUMBENT_BND(KPNode, int);

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
    std::cout << "Error in file parsing" << std::endl;
    hpx::finalize();
    return EXIT_FAILURE;
  }

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

  auto space = std::make_pair(profits, weights);

  int numItems = problem.items.size();

  // Check profit density ordering (required for bounding to work correctly)
  for (int i = 0; i < numItems - 1; i++) {
    if (profits[i] / weights[i] < profits[i+1] / weights[i+1]) {
      std::cout << "Input not in profit density ordering" << std::endl;
      hpx::finalize();
      return EXIT_FAILURE;
    }
  }

  KPSolution initSol = {numItems, problem.capacity, std::vector<int>(), 0, 0};

  std::vector<int> initRem;
  for (int i = 0; i < numItems; i++) {
    initRem.push_back(i);
  }
  KPNode root {initSol, initRem};

  auto sol = root;
  auto skeletonType = opts["skeleton"].as<std::string>();
  if (skeletonType == "seq") {
    sol = YewPar::Skeletons::Seq<GenNode<NUMITEMS>,
                                 YewPar::Skeletons::API::BnB,
                                 YewPar::Skeletons::API::BoundFunction<bnd_func> >
          ::search(space, root);
  } else if (skeletonType == "depthbounded") {
    auto spawnDepth = opts["spawn-depth"].as<unsigned>();
    YewPar::Skeletons::API::Params<int> searchParameters;
    searchParameters.spawnDepth = spawnDepth;
    sol = YewPar::Skeletons::DepthSpawns<GenNode<NUMITEMS>,
                                         YewPar::Skeletons::API::BnB,
                                         YewPar::Skeletons::API::BoundFunction<bnd_func> >
          ::search(space, root, searchParameters);
  } else if (skeletonType == "ordered") {
    auto spawnDepth = opts["spawn-depth"].as<unsigned>();
    YewPar::Skeletons::API::Params<int> searchParameters;
    searchParameters.spawnDepth = spawnDepth;
    sol = YewPar::Skeletons::Ordered<GenNode<NUMITEMS>,
                                     YewPar::Skeletons::API::BnB,
                                     YewPar::Skeletons::API::BoundFunction<bnd_func> >
          ::search(space, root, searchParameters);
  } else {
    std::cout << "Invalid skeleton type\n";
    hpx::finalize();
    return EXIT_FAILURE;
  }

  auto finalSol = sol.sol;
  std::cout << "Final Profit: " << finalSol.profit << std::endl;
  std::cout << "Final Weight: " << finalSol.weight << std::endl;
  std::cout << "Expected Result: " << std::boolalpha << (finalSol.profit == problem.expectedResult) << std::endl;
  std::cout << "Items: ";
  for (auto const & i : finalSol.items) {
    std::cout << i << " ";
  }
  std::cout << std::endl;

  return hpx::finalize();
}

int main(int argc, char* argv[]) {
  boost::program_options::options_description
    desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

  desc_commandline.add_options()
    ( "skeleton",
      boost::program_options::value<std::string>()->default_value("seq"),
      "Type of skeleton to use: seq, depthbounded, ordered"
    )
    ( "input-file,f",
      boost::program_options::value<std::string>(),
      "Input problem"
    )
    ( "spawn-depth,d",
      boost::program_options::value<unsigned>()->default_value(0),
      "Depth in the tree to spawn until (for parallel skeletons only)"
    );

  return hpx::init(desc_commandline, argc, argv);
}
