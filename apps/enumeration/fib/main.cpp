#include <hpx/hpx_init.hpp>

#include <vector>
#include <memory>
#include <chrono>

#include "skeletons/Seq.hpp"
#include "skeletons/DepthSpawning.hpp"
#include "skeletons/StackStealing.hpp"
#include "util/NodeGenerator.hpp"

// Fib doesn't have a space
struct Empty {};

struct NodeGen : YewPar::NodeGenerator<std::uint64_t, Empty> {
  std::uint64_t n;
  unsigned i = 1;

  NodeGen() = default;

  NodeGen(const Empty & space, const std::uint64_t & n) : n(n) {
    this->numChildren = 2;
  }

  std::uint64_t next() override {
    auto res = n - i;
    ++i;
    return res;
  }
};

NodeGen generateChildren(const Empty & space, const std::uint64_t & n) {
  return NodeGen(space, n);
}

#define MAX_DEPTH 50

using ss_skel = YewPar::Skeletons::StackStealing<NodeGen,
                                                 YewPar::Skeletons::API::CountNodes,
                                                 YewPar::Skeletons::API::DepthBounded>;
using ntype = std::uint64_t;
using cfunc  = ss_skel::SubTreeTask;
REGISTER_SEARCHMANAGER(ntype, cfunc);

int hpx_main(boost::program_options::variables_map & opts) {
  auto spawnDepth = opts["spawn-depth"].as<unsigned>();
  auto maxDepth   = opts["until-depth"].as<unsigned>();
  auto skeleton   = opts["skeleton-type"].as<std::string>();

  auto start_time = std::chrono::steady_clock::now();

  std::vector<std::uint64_t> counts;
  if (skeleton == "seq") {
    YewPar::Skeletons::API::Params<> searchParameters;
    searchParameters.maxDepth = maxDepth;
    counts = YewPar::Skeletons::Seq<NodeGen,
                                    YewPar::Skeletons::API::CountNodes,
                                    YewPar::Skeletons::API::DepthBounded>
             ::search(Empty(), maxDepth - 1, searchParameters);
  } else if (skeleton == "dist"){
    YewPar::Skeletons::API::Params<> searchParameters;
    searchParameters.maxDepth   = maxDepth;
    searchParameters.spawnDepth = spawnDepth;
    counts = YewPar::Skeletons::DepthSpawns<NodeGen,
                                    YewPar::Skeletons::API::CountNodes,
                                    YewPar::Skeletons::API::DepthBounded>
             ::search(Empty(), maxDepth - 1, searchParameters);
  } else if (skeleton == "genstack"){
    YewPar::Skeletons::API::Params<> searchParameters;
    searchParameters.maxDepth   = maxDepth;
    counts = ss_skel::search(Empty(), maxDepth - 1, searchParameters);
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
        boost::program_options::value<std::string>()->default_value("seq"),
        "Which skeleton to use"
        )
      ( "spawn-depth,s",
        boost::program_options::value<unsigned>()->default_value(0),
        "Depth in the tree to spawn until (for parallel skeletons only)"
        )
      ( "until-depth,d",
        boost::program_options::value<unsigned>()->default_value(0),
        "Depth in the tree to count until"
        );

  hpx::register_startup_function(&Workstealing::Policies::SearchManagerPerf::registerPerformanceCounters);

  return hpx::init(desc_commandline, argc, argv);
}
