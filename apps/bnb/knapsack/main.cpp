#include <algorithm>
#include <string>
#include <regex>
#include <exception>

#include <hpx/hpx.hpp>
#include <hpx/hpx_init.hpp>

#include "bnb/bnb-seq.hpp"
#include "bnb/bnb-par.hpp"
#include "bnb/bnb-dist.hpp"
#include "bnb/macros.hpp"

#include "knapsack.hpp"

#ifndef NUMITEMS
#define NUMITEMS 50
#endif

// Actions for HPX (PAR)
HPX_PLAIN_ACTION(generateChoices<NUMITEMS>, gen_action)
HPX_PLAIN_ACTION(upperBound<NUMITEMS>, bnd_action)
YEWPAR_CREATE_BNB_PAR_ACTION(childTask_act, KPSpace<NUMITEMS>, KPSolution, int, std::vector<int>, gen_action, bnd_action)

typedef std::vector<int> vecint;
REGISTER_INCUMBENT(KPSolution, int, vecint);

// Actions for HPX (DIST)
YEWPAR_CREATE_BNB_DIST_ACTION(childTaskDist_act, KPSpace<NUMITEMS>, KPSolution, int, std::vector<int>, gen_action, bnd_action)

REGISTER_REGISTRY(KPSpace<NUMITEMS>, KPSolution, int, vecint);

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
  // TODO: Proper lower case conversion
  const std::vector<std::string> skeletonTypes = {"seq", "par", "dist" };

  auto skeletonType = opts["skeleton"].as<std::string>();
  auto found = std::find(std::begin(skeletonTypes), std::end(skeletonTypes), skeletonType);
  if (found == std::end(skeletonTypes)) {
    std::cout << "Invalid skeleton type option. Should be: seq, par or dist" << std::endl;
    hpx::finalize();
    return EXIT_FAILURE;
  }

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
  auto root  = hpx::util::make_tuple(initSol, 0, initRem);

  auto sol = root;
  if (skeletonType == "seq") {
    sol = skeletons::BnB::Seq::search<KPSpace<NUMITEMS>, KPSolution, int, std::vector<int> >
      (space, root, generateChoices<NUMITEMS>, upperBound<NUMITEMS>);
  }

  if (skeletonType == "par") {
    auto spawnDepth = opts["spawn-depth"].as<int>();
    sol = skeletons::BnB::Par::search<KPSpace<NUMITEMS>, KPSolution, int, std::vector<int>, gen_action, bnd_action, childTask_act >
      (spawnDepth, space, root);
  }

  if (skeletonType == "dist") {
    auto spawnDepth = opts["spawn-depth"].as<int>();
    sol = skeletons::BnB::Dist::search<KPSpace<NUMITEMS>, KPSolution, int, std::vector<int>, gen_action, bnd_action, childTaskDist_act>
      (spawnDepth, space, root);
  }

  auto finalSol = hpx::util::get<0>(sol);
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
      "Type of skeleton to use: seq, par, dist"
      );

  return hpx::init(desc_commandline, argc, argv);
}
