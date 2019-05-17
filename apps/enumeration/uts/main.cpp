#include <hpx/hpx_init.hpp>
#include <hpx/include/iostreams.hpp>

// We just use the default RNG for simplicity
#define BRG_RNG
#include "uts-rng/rng.h"

#include "YewPar.hpp"
#include "skeletons/Seq.hpp"
#include "skeletons/DepthSpawning.hpp"
#include "skeletons/StackStealing.hpp"
#include "skeletons/Budget.hpp"

enum GeometricType {
  LINEAR = 0, CYCLIC, FIXED, EXPDEC
};

// UTS state stores program parameters
struct UTSState {
  double rootBF;
  double nonLeafBF;
  double nonLeafProb;

  // For geometric trees
  int gen_mx = 6;
  GeometricType geoType = GeometricType::LINEAR;

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & rootBF;
    ar & nonLeafBF;
    ar & nonLeafProb;
    ar & gen_mx;
    ar & geoType;
  }
};

enum TreeType {
  BINOMIAL, GEOMETRIC
};

// Serialising RNG state
namespace hpx { namespace serialization {
template<class Archive>
void serialize(Archive & ar, state_t & x, const unsigned int version) {
  ar & x.state;
}}}

struct UTSNode {
  bool isRoot;
  int depth;
  state_t rngstate; //rng state (uint_8t state[20])

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & isRoot;
    ar & depth;
    ar & rngstate;
  }
};

template <TreeType t>
struct NodeGen {};

template <>
struct NodeGen<TreeType::BINOMIAL> : YewPar::NodeGenerator<UTSNode, UTSState> {
  UTSNode parent;
  UTSState params;
  int i = 0;

  // Interpret 32 bit positive integer as value on [0,1)
  // From UTS Codebase
  double rng_toProb(int n) {
    if (n < 0) {
      printf("*** toProb: rand n = %d out of range\n",n);
    }
    return ((n<0)? 0.0 : ((double) n)/2147483648.0);
  }

  int calcNumChildren() {
    if (parent.isRoot) {
      return params.rootBF;
    } else {
      double d = rng_toProb(rng_rand(parent.rngstate.state));
      return (d < params.nonLeafProb) ? params.nonLeafBF : 0;
    }
  }

  NodeGen() { this->numChildren = 0; }

  NodeGen(const UTSState & params, const UTSNode & parent) : params(params), parent(parent) {
    this->numChildren = calcNumChildren();
  }

  UTSNode next() override {
    UTSNode child { false, parent.depth + 1 };
    rng_spawn(parent.rngstate.state, child.rngstate.state, i);
    ++i;

    return child;
  }
};

template <>
struct NodeGen<TreeType::GEOMETRIC> : YewPar::NodeGenerator<UTSNode, UTSState> {
  UTSNode parent;
  UTSState params;
  int i = 0;

  NodeGen() { this->numChildren = 0; }
  NodeGen(const UTSState & params, const UTSNode & parent) : params(params), parent(parent) {
    this->numChildren = calcNumChildren();
  }

  double rng_toProb(int n) {
    if (n < 0) {
      printf("*** toProb: rand n = %d out of range\n",n);
    }
    return ((n<0)? 0.0 : ((double) n)/2147483648.0);
  }

  int calcNumChildren () {
    auto depth = parent.depth;
    auto branchFactor = params.rootBF;

    if (!parent.isRoot) {
      switch (params.geoType) {
        case GeometricType::CYCLIC:
          if (depth > 5 * params.gen_mx){
            branchFactor = 0.0;
            break;
          }
          branchFactor = pow(params.rootBF, sin(2.0*3.141592653589793*(double) depth / (double) params.gen_mx));
          break;
        case GeometricType::FIXED:
          branchFactor = (depth < params.gen_mx) ? params.rootBF : 0;
          break;
        case EXPDEC:
          branchFactor = params.rootBF * pow((double) depth, -log(params.rootBF)/log((double) params.gen_mx));
          break;
        case GeometricType::LINEAR:
        default:
          branchFactor =  params.rootBF * (1.0 - (double)depth / (double) params.gen_mx);
          break;
      }
    }

    auto p = 1.0 / (1.0 + branchFactor);
    auto u = rng_toProb(rng_rand(parent.rngstate.state));

    return (int) floor(log(1 - u) / log(1 - p));
  }

  UTSNode next() override {
    UTSNode child { false, parent.depth + 1 };
    rng_spawn(parent.rngstate.state, child.rngstate.state, i);
    ++i;

    return child;
  }
};

#ifndef UTS_MAX_TREE_DEPTH
#define UTS_MAX_TREE_DEPTH 20000
#endif

std::vector<std::string> treeTypes = {"binomial", "geometric"};

int hpx_main(boost::program_options::variables_map & opts) {
  auto spawnDepth = opts["spawn-depth"].as<unsigned>();
  auto maxDepth   = opts["until-depth"].as<unsigned>();
  auto skeleton   = opts["skeleton"].as<std::string>();
  auto treeType   = opts["uts-t"].as<std::string>();

  auto start_time = std::chrono::steady_clock::now();

  UTSState params { opts["uts-b"].as<double>(), opts["uts-m"].as<double>(), opts["uts-q"].as<double>(),
        opts["uts-d"].as<int>(), static_cast<GeometricType>(opts["uts-a"].as<int>()) };

  UTSNode root { true, 0 };
  rng_init(root.rngstate.state, opts["uts-r"].as<int>());

  std::vector<std::uint64_t> counts;
  if (treeType == "binomial") {
    if (skeleton == "seq") {
      counts = YewPar::Skeletons::Seq<NodeGen<TreeType::BINOMIAL>,
                                      YewPar::Skeletons::API::CountNodes>
               ::search(params, root);
    } else if (skeleton == "depthbounded") {
      YewPar::Skeletons::API::Params<> searchParameters;
      searchParameters.spawnDepth = spawnDepth;
      counts = YewPar::Skeletons::DepthSpawns<NodeGen<TreeType::BINOMIAL>,
                                              YewPar::Skeletons::API::CountNodes>
               ::search(params, root, searchParameters);
    } else if (skeleton == "stacksteal") {
      YewPar::Skeletons::API::Params<> searchParameters;
      searchParameters.stealAll = static_cast<bool>(opts.count("chunked"));
      counts = YewPar::Skeletons::StackStealing<NodeGen<TreeType::BINOMIAL>,
                                                YewPar::Skeletons::API::CountNodes,
                                                YewPar::Skeletons::API::DepthBounded,
                                                YewPar::Skeletons::API::MaxStackDepth<
                                                  std::integral_constant<unsigned, UTS_MAX_TREE_DEPTH> > >
               ::search(params, root, searchParameters);
    } else if (skeleton == "budget") {
      YewPar::Skeletons::API::Params<> searchParameters;
      searchParameters.backtrackBudget = opts["backtrack-budget"].as<unsigned>();
      counts = YewPar::Skeletons::Budget<NodeGen<TreeType::BINOMIAL>,
                                         YewPar::Skeletons::API::CountNodes,
                                         YewPar::Skeletons::API::DepthBounded,
                                         YewPar::Skeletons::API::MaxStackDepth<
                                           std::integral_constant<unsigned, UTS_MAX_TREE_DEPTH> > >
          ::search(params, root, searchParameters);
    }
  } else if (treeType == "geometric"){
    if (skeleton == "seq") {
      YewPar::Skeletons::API::Params<> searchParameters;
      counts = YewPar::Skeletons::Seq<NodeGen<TreeType::GEOMETRIC>,
                                      YewPar::Skeletons::API::CountNodes>
               ::search(params, root, searchParameters);
    } else if (skeleton == "depthbounded") {
      YewPar::Skeletons::API::Params<> searchParameters;
      searchParameters.spawnDepth = spawnDepth;
      counts = YewPar::Skeletons::DepthSpawns<NodeGen<TreeType::GEOMETRIC>,
                                              YewPar::Skeletons::API::CountNodes>
               ::search(params, root, searchParameters);
    } else if (skeleton == "stacksteal") {
      YewPar::Skeletons::API::Params<> searchParameters;
      searchParameters.stealAll = static_cast<bool>(opts.count("chunked"));
      counts = YewPar::Skeletons::StackStealing<NodeGen<TreeType::GEOMETRIC>,
                                                YewPar::Skeletons::API::CountNodes,
                                                YewPar::Skeletons::API::DepthBounded,
                                                YewPar::Skeletons::API::MaxStackDepth<
                                                  std::integral_constant<unsigned, UTS_MAX_TREE_DEPTH> > >
               ::search(params, root, searchParameters);
    } else if (skeleton == "budget") {
      YewPar::Skeletons::API::Params<> searchParameters;
      searchParameters.backtrackBudget = opts["backtrack-budget"].as<unsigned>();
      counts = YewPar::Skeletons::Budget<NodeGen<TreeType::GEOMETRIC>,
                                         YewPar::Skeletons::API::CountNodes,
                                         YewPar::Skeletons::API::DepthBounded,
                                         YewPar::Skeletons::API::MaxStackDepth<
                                           std::integral_constant<unsigned, UTS_MAX_TREE_DEPTH> > >
          ::search(params, root, searchParameters);
    }
  } else {
    hpx::cout << "Invalid tree type\n";
  }

  auto overall_time = std::chrono::duration_cast<std::chrono::milliseconds>
                      (std::chrono::steady_clock::now() - start_time);

  hpx::cout << "Total Nodes: " << std::accumulate<std::vector<std::uint64_t>::iterator, std::uint64_t>(counts.begin(), counts.end(), 0) << hpx::endl;

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
      ( "spawn-depth,s",
        boost::program_options::value<unsigned>()->default_value(0),
        "Depth in the tree to spawn until (for parallel skeletons only)"
        )
      ( "until-depth,d",
        boost::program_options::value<unsigned>()->default_value(0),
        "Depth in the tree to count until"
        )
      ( "backtrack-budget,b",
        boost::program_options::value<unsigned>()->default_value(500),
        "Number of backtracks before spawning work"
        )
      ("chunked", "Use chunking with stack stealing")
      // UTS Options
      //LINEAR, CYCLIC, FIXED, EXPDEC
      ( "uts-t", boost::program_options::value<std::string>()->default_value("binomial"), "Which tree type to use" )
      ( "uts-b", boost::program_options::value<double>()->default_value(4.0), "Root branching factor" )
      ( "uts-q", boost::program_options::value<double>()->default_value(15.0 / 64.0), "BIN: Probability of non-leaf node" )
      ( "uts-m", boost::program_options::value<double>()->default_value(4.0), "BIN: Number of children for non-leaf node" )
      ( "uts-r", boost::program_options::value<int>()->default_value(0), "root seed" )
      ( "uts-d", boost::program_options::value<int>()->default_value(6), "GEO: Tree Depth" )
      ( "uts-a", boost::program_options::value<int>()->default_value(0), "GEO: Tree shape function (0: LINEAR, 1: CYCLIC, 2: FIXED, 3: EXPDEC)" )
      ;

  YewPar::registerPerformanceCounters();

  return hpx::init(desc_commandline, argc, argv);
}
