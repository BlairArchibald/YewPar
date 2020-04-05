/* Original Numerical Semigroups Code by Florent Hivert: https://www.lri.fr/~hivert/
   Link: https://github.com/hivert/NumericMonoid/blob/master/src/Cilk++/monoid.hpp
 */

#include <hpx/hpx_init.hpp>
#include <hpx/include/iostreams.hpp>

#include <vector>
#include <chrono>

#include "YewPar.hpp"
#include "skeletons/Seq.hpp"
#include "skeletons/DepthBounded.hpp"
#include "skeletons/StackStealing.hpp"
#include "skeletons/Budget.hpp"

#include "monoid.hpp"

// Numerical Semigroups don't have a space
struct Empty {};

struct NodeGen : YewPar::NodeGenerator<Monoid, Empty> {
  Monoid group;
  generator_iter<CHILDREN> it;

  NodeGen(const Empty &, const Monoid & s) : group(s), it(generator_iter<CHILDREN>(s)){
    this->numChildren = it.count(group);
    it.move_next(group); // Original code skips begin
  }

  Monoid next() override {
    auto res = remove_generator(group, it.get_gen());
    it.move_next(group);
    return res;
  }
};

int hpx_main(boost::program_options::variables_map & opts) {
  auto spawnDepth = opts["spawn-depth"].as<unsigned>();
  auto maxDepth   = opts["genus"].as<unsigned>();
  auto skeleton   = opts["skeleton"].as<std::string>();
  //auto stealAll   = opts["stealall"].as<bool>();

  Monoid root;
  init_full_N(root);

  auto start_time = std::chrono::steady_clock::now();

  std::vector<std::uint64_t> counts;
  if (skeleton == "depthbounded") {
    YewPar::Skeletons::API::Params<> searchParameters;
    searchParameters.maxDepth   = maxDepth;
    searchParameters.spawnDepth = spawnDepth;
    counts = YewPar::Skeletons::DepthBounded<NodeGen,
                                              YewPar::Skeletons::API::CountNodes,
                                              YewPar::Skeletons::API::DepthLimited>
              ::search(Empty(), root, searchParameters);
  } else if (skeleton == "stacksteal"){
    YewPar::Skeletons::API::Params<> searchParameters;
    searchParameters.maxDepth = maxDepth;
    searchParameters.stealAll = static_cast<bool>(opts.count("chunked"));
    // EXTENSION
    if (opts.count("nodes")) {
      counts = YewPar::Skeletons::StackStealing<NodeGen,
                                              YewPar::Skeletons::API::CountNodes,
                                              YewPar::Skeletons::API::NodeCounts,
                                              YewPar::Skeletons::API::DepthLimited>
              ::search(Empty(), root, searchParameters);
    } else if (opts.count("backtracks")) {
      counts = YewPar::Skeletons::StackStealing<NodeGen,
                                              YewPar::Skeletons::API::CountNodes,
                                              YewPar::Skeletons::API::Backtracks,
                                              YewPar::Skeletons::API::DepthLimited>
              ::search(Empty(), root, searchParameters);
    } else if (opts.count("regularity")) {
      counts = YewPar::Skeletons::StackStealing<NodeGen,
                                              YewPar::Skeletons::API::CountNodes,
                                              YewPar::Skeletons::API::Regularity,
                                              YewPar::Skeletons::API::DepthLimited>
              ::search(Empty(), root, searchParameters);
    } else {
      counts = YewPar::Skeletons::StackStealing<NodeGen,
                                              YewPar::Skeletons::API::CountNodes,
                                              YewPar::Skeletons::API::DepthLimited>
              ::search(Empty(), root, searchParameters);
    }
    // END EXTENSION
  } else if (skeleton == "budget"){
    YewPar::Skeletons::API::Params<> searchParameters;
    searchParameters.backtrackBudget = opts["backtrack-budget"].as<unsigned>();
    searchParameters.maxDepth   = maxDepth;
    // EXTENSION
    if (opts.count("nodes")) {
      counts = YewPar::Skeletons::Budget<NodeGen,
                                       YewPar::Skeletons::API::CountNodes,
                                       YewPar::Skeletons::API::NodeCounts,
                                       YewPar::Skeletons::API::DepthLimited>
        ::search(Empty(), root, searchParameters);
    } else if (opts.count("backtracks")) {
      counts = YewPar::Skeletons::Budget<NodeGen,
                                       YewPar::Skeletons::API::CountNodes,
                                       YewPar::Skeletons::API::Backtracks,
                                       YewPar::Skeletons::API::DepthLimited>
        ::search(Empty(), root, searchParameters);
    } else if (opts.count("regularity")) {
      counts = YewPar::Skeletons::Budget<NodeGen,
                                       YewPar::Skeletons::API::CountNodes,
                                       YewPar::Skeletons::API::Regularity,
                                       YewPar::Skeletons::API::DepthLimited>
        ::search(Empty(), root, searchParameters);
    } else {
      counts = YewPar::Skeletons::Budget<NodeGen,
                                       YewPar::Skeletons::API::CountNodes,
                                       YewPar::Skeletons::API::DepthLimited>
        ::search(Empty(), root, searchParameters);
    }
    // END EXTENSION
  } else {
    hpx::cout << "Invalid skeleton type: " << skeleton << hpx::endl;
    return hpx::finalize();
  }

  auto overall_time = std::chrono::duration_cast<std::chrono::milliseconds>
                      (std::chrono::steady_clock::now() - start_time);

  hpx::cout << "Results Table: " << hpx::endl;
  for (auto i = 0; i <= maxDepth; ++i) {
    hpx::cout << i << ": " << counts[i] << hpx::endl;
  }
  hpx::cout << "=====" << hpx::endl;
  hpx::cout << "cpu = " << overall_time.count() << hpx::endl;

  return hpx::finalize();
}

int main(int argc, char* argv[]) {
  boost::program_options::options_description
    desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

  desc_commandline.add_options()
    ( "skeleton",
      boost::program_options::value<std::string>()->default_value("seq"),
      "Which skeleton to use: seq, depthbound, stacksteal, or budget"
    )
    ( "spawn-depth,d",
      boost::program_options::value<unsigned>()->default_value(0),
      "Depth in the tree to spawn until (for parallel skeletons only)"
    )
    ( "genus,g",
      boost::program_options::value<unsigned>()->default_value(0),
      "Depth in the tree to count until"
    )
    ( "backtrack-budget,b",
      boost::program_options::value<unsigned>()->default_value(500),
      "Number of backtracks before spawning work"
    )
    ( "verbose,v",
      boost::program_options::value<bool>()->default_value(false),
      "Enable verbose output"
    )
    ("chunked", "Use chunking with stack stealing")
    // EXTENSION
    ("backtracks", "Collect the backtracks metric")
    ("nodes", "Collect the backtracks metric")
    ("regularity", "Collect the backtracks metric");
    // END EXTENSION

  YewPar::registerPerformanceCounters();

  return hpx::init(desc_commandline, argc, argv);
}
