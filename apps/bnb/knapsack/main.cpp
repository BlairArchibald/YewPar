#include <algorithm>
#include <string>

#include <hpx/hpx.hpp>
#include <hpx/hpx_init.hpp>

#include "bnb/bnb-seq.hpp"
#include "bnb/bnb-par.hpp"
#include "bnb/bnb-dist.hpp"

#include "knapsack.hpp"

auto const numItems = 10;

// Actions for HPX (PAR)
HPX_PLAIN_ACTION(generateChoices<numItems>, gen_action)
HPX_PLAIN_ACTION(upperBound<numItems>, bnd_action)

// FIXME: I should define a MACRO for this setup
struct childTask_act : hpx::actions::make_action<
  decltype(&skeletons::BnB::Par::searchChildTask<KPSpace<numItems>, KPSolution, int, std::vector<int>, gen_action, bnd_action, childTask_act>),
  &skeletons::BnB::Par::searchChildTask<KPSpace<numItems>, KPSolution, int, std::vector<int>, gen_action, bnd_action, childTask_act>,
  childTask_act>::type {};

typedef std::vector<int> vecint;
REGISTER_INCUMBENT(KPSolution, int, vecint);

// Actions for HPX (DIST)
struct childTaskDist_act : hpx::actions::make_action<
  decltype(&skeletons::BnB::Dist::searchChildTask<KPSpace<numItems>, KPSolution, int, std::vector<int>, gen_action, bnd_action, childTaskDist_act>),
    &skeletons::BnB::Dist::searchChildTask<KPSpace<numItems>, KPSolution, int, std::vector<int>, gen_action, bnd_action, childTaskDist_act>,
    childTaskDist_act>::type {};

REGISTER_REGISTRY(KPSpace<numItems>, int);

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

  // TODO: Read instances from file, simple test case for now
  /* Simple test params P01 from https://people.sc.fsu.edu/~jburkardt/datasets/knapsack_01/knapsack_01.html/ */
  auto const capacity = 165;

  // Sol: 1 1 1 1 0 1 0 0 0 0
  std::array<int, numItems> profits = {92, 57, 49, 68, 60, 43, 67, 84, 87, 72};
  std::array<int, numItems> weights = {23, 31, 29, 44, 53, 38, 63, 85, 89, 82};
  auto space = std::make_pair(profits, weights);

  // Check profit density ordering (required for bounding to work correctly)
  for (int i = 0; i < numItems - 1; i++) {
    if (profits[i] / weights[i] < profits[i+1] / weights[i+1]) {
      std::cout << "Warning: Input not in profit density ordering" << std::endl;
    }
  }

  KPSolution initSol = {numItems, capacity, std::vector<int>(), 0, 0};

  std::vector<int> initRem;
  for (int i = 0; i < numItems; i++) {
    initRem.push_back(i);
  }
  auto root  = hpx::util::make_tuple(initSol, 0, initRem);

  auto sol = root;
  if (skeletonType == "seq") {
    sol = skeletons::BnB::Seq::search<KPSpace<numItems>, KPSolution, int, std::vector<int> >
      (space, root, generateChoices<numItems>, upperBound<numItems>);
  }

  if (skeletonType == "par") {
    auto spawnDepth = opts["spawn-depth"].as<std::uint64_t>();
    sol = skeletons::BnB::Par::search<KPSpace<numItems>, KPSolution, int, std::vector<int>, gen_action, bnd_action, childTask_act >
      (spawnDepth, space, root);
  }

  if (skeletonType == "dist") {
    auto spawnDepth = opts["spawn-depth"].as<std::uint64_t>();
    sol = skeletons::BnB::Dist::search<KPSpace<numItems>, KPSolution, int, std::vector<int>, gen_action, bnd_action, childTaskDist_act>
      (spawnDepth, space, root);
  }

  auto finalSol = hpx::util::get<0>(sol);
  std::cout << "Final Profit: " << finalSol.profit << std::endl;
  std::cout << "Final Weight: " << finalSol.weight << std::endl;
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
    ( "skeleton",
      boost::program_options::value<std::string>()->default_value("seq"),
      "Type of skeleton to use: seq, par, dist"
      );

  return hpx::init(desc_commandline, argc, argv);
}
