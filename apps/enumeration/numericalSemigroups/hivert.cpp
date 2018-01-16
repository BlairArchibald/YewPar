/* Original Numerical Semigroups Code by Florent Hivert: https://www.lri.fr/~hivert/
   Link: https://github.com/hivert/NumericMonoid/blob/master/src/Cilk++/monoid.hpp
 */

#include <hpx/hpx_init.hpp>

#include <vector>
#include <chrono>

#include "skeletons/Seq.hpp"
#include "skeletons/DepthSpawning.hpp"
#include "skeletons/StackStealing.hpp"

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

using ss_skel = YewPar::Skeletons::StackStealing<NodeGen,
                                                 YewPar::Skeletons::API::CountNodes,
                                                 YewPar::Skeletons::API::DepthBounded>;
using cFunc = ss_skel::SubTreeTask;
REGISTER_SEARCHMANAGER(Monoid, cFunc);

int hpx_main(boost::program_options::variables_map & opts) {
  auto spawnDepth = opts["spawn-depth"].as<unsigned>();
  auto maxDepth   = opts["until-depth"].as<unsigned>();
  auto skeleton   = opts["skeleton-type"].as<std::string>();
  auto stealAll   = opts["stealall"].as<bool>();

  Monoid root;
  init_full_N(root);

  auto start_time = std::chrono::steady_clock::now();

  std::vector<std::uint64_t> counts;
  if (skeleton == "seq") {
    YewPar::Skeletons::API::Params<> searchParameters;
    searchParameters.maxDepth = maxDepth;
    counts = YewPar::Skeletons::Seq<NodeGen,
                                    YewPar::Skeletons::API::CountNodes,
                                    YewPar::Skeletons::API::DepthBounded>
             ::search(Empty(), root, searchParameters);
  } else if (skeleton == "dist") {
    YewPar::Skeletons::API::Params<> searchParameters;
    searchParameters.maxDepth   = maxDepth;
    searchParameters.spawnDepth = spawnDepth;
    counts = YewPar::Skeletons::DepthSpawns<NodeGen,
                                            YewPar::Skeletons::API::CountNodes,
                                            YewPar::Skeletons::API::DepthBounded>
             ::search(Empty(), root, searchParameters);
  } else if (skeleton == "genstack"){
    YewPar::Skeletons::API::Params<> searchParameters;
    searchParameters.maxDepth   = maxDepth;
    counts = ss_skel::search(Empty(), root, searchParameters);
  } else {
    std::cout << "Invalid skeleton type: " << skeleton << std::endl;
    return hpx::finalize();
  }

  auto overall_time = std::chrono::duration_cast<std::chrono::milliseconds>
                      (std::chrono::steady_clock::now() - start_time);

  std::cout << "Results Table: " << std::endl;
  for (auto i = 0; i <= maxDepth; ++i) {
    std::cout << i << ": " << counts[i] << std::endl;
  }
  std::cout << "=====" << std::endl;
  std::cout << "cpu = " << overall_time.count() << std::endl;

  return hpx::finalize();
}

int main(int argc, char* argv[]) {
  boost::program_options::options_description
    desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

  desc_commandline.add_options()
    ( "skeleton-type",
      boost::program_options::value<std::string>()->default_value("dist"),
      "Which skeleton to use"
    )
    ( "spawn-depth,s",
      boost::program_options::value<unsigned>()->default_value(0),
      "Depth in the tree to spawn until (for parallel skeletons only)"
    )
    ( "until-depth,d",
      boost::program_options::value<unsigned>()->default_value(0),
      "Depth in the tree to count until"
    )
    ( "verbose,v",
      boost::program_options::value<bool>()->default_value(false),
      "Enable verbose output"
    )
    ( "stealall",
      boost::program_options::value<bool>()->default_value(false),
      "Steal all when chunking"
    );

  hpx::register_startup_function(&Workstealing::Policies::SearchManagerPerf::registerPerformanceCounters);

  return hpx::init(desc_commandline, argc, argv);
}
