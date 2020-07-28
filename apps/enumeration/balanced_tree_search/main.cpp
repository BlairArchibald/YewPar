#include <hpx/hpx_init.hpp>

#include "skeletons/DepthSpawning.hpp"

struct Node {};
struct Config {
  std::uint64_t numChildren; // children at each depth

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & numChildren;
  }
};

struct NodeGen : YewPar::NodeGenerator<Node, Config> {
  Node nextNode;

  NodeGen(const Config & space, Node) {
    this->numChildren = space.numChildren;
  }

  // Just return a fixed number of nodes
  Node next() override {
    return nextNode;
  }
};

int hpx_main(boost::program_options::variables_map & opts) {
  auto spawnDepth = opts["spawn-depth"].as<unsigned>();
  auto maxDepth   = opts["until-depth"].as<unsigned>();
  auto nchildren  = opts["nchildren"].as<unsigned>();
  auto skeleton   = opts["skeleton"].as<std::string>();

  auto start_time = std::chrono::steady_clock::now();

  Config cfg {nchildren};
  Node root;

  std::vector<std::uint64_t> counts;
  if (skeleton == "depthbounded") {
    YewPar::Skeletons::API::Params<> searchParameters;
    searchParameters.spawnDepth = spawnDepth;
    searchParameters.maxDepth = maxDepth;
    counts = YewPar::Skeletons::DepthSpawns<NodeGen,
                                            YewPar::Skeletons::API::CountNodes,
                                            YewPar::Skeletons::API::DepthBounded>
      ::search(cfg, root, searchParameters);
  } else {
    std::cout << "Skeleton type not supported\n";
  }

  auto overall_time = std::chrono::duration_cast<std::chrono::milliseconds>
                      (std::chrono::steady_clock::now() - start_time);

  std::cout << "Total Nodes: " << std::accumulate(counts.begin(), counts.end(), 0) << std::endl;
  std::cout << "=====" << std::endl;
  std::cout << "cpu = " << overall_time.count() << std::endl;

  return hpx::finalize();
}

int main(int argc, char* argv[]) {
  boost::program_options::options_description
      desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

  desc_commandline.add_options()
      ( "skeleton",
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
        )
      ( "nchildren,c",
        boost::program_options::value<unsigned>()->default_value(2),
        "Number of children per node (balanced)"
        )
      ;

  return hpx::init(desc_commandline, argc, argv);
}
