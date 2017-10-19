#include <hpx/hpx.hpp>
#include <hpx/hpx_init.hpp>

#include "enumerate/skeletons.hpp"
#include "enumerate/dist-nodegen-stack.hpp"

#include "util/func.hpp"

// We just use the default RNG for simplicity
#define BRG_RNG
#include "uts-rng/rng.h"

// UTS doesn't have a space
struct Empty {};

enum TreeType {
  BINOMIAL
};

// Serialising RNG state
namespace hpx { namespace serialization {
template<class Archive>
void serialize(Archive & ar, state_t & x, const unsigned int version) {
  ar & x;
}}}

// FIXME: We might also need to carry the height since some nodes have functions based on heights
// UTS Nodes only need to carry RNG state
struct UTSNode {
  bool isRoot;
  state_t rngstate; //rng state (uint_8t state[20])

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & rngstate;
  }
};



template <TreeType TreeType>
struct NodeGen;

template <>
struct NodeGen<BINOMIAL> : skeletons::Enum::NodeGenerator<Empty, UTSNode> {
  UTSNode parent;
  int i = 0;

  // FIXME: Put this in the local state instead of hardcoding?
  const int leafBF = 4;
  const int nonLeafBF = 4;
  const double nonLeafProb = 15.0 / 64.0;

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
      return leafBF;
    } else {
      double d = rng_toProb(rng_rand(parent.rngstate.state));
      return (d < nonLeafProb) ? nonLeafBF : 0;
    }
  }

  NodeGen(const UTSNode & parent) : parent(parent) {
    this->numChildren = calcNumChildren();
  }

  UTSNode next() override {
    UTSNode child;
    rng_spawn(parent.rngstate.state, child.rngstate.state, i);
    ++i;

    return child;
  }
};

//template <TreeType TreeType>
auto generateChildren(const Empty & space, const UTSNode & n) {
  return NodeGen<BINOMIAL>(n);
  // constexpr if(TreeType == BINOMIAL) {
  //   return NodeGen<BINOMIAL>(n);
  // }
}

typedef func<decltype(&generateChildren), &generateChildren> genChildren_func;
REGISTER_ENUM_REGISTRY(Empty, UTSNode)

std::vector<std::string> treeTypes = {"binomial"};

int hpx_main(boost::program_options::variables_map & opts) {
  auto spawnDepth = opts["spawn-depth"].as<unsigned>();
  auto maxDepth   = opts["until-depth"].as<unsigned>();
  auto skeleton   = opts["skeleton-type"].as<std::string>();

  auto start_time = std::chrono::steady_clock::now();

  UTSNode root { true };
  rng_init(root.rngstate.state, 0);

  std::vector<std::uint64_t> counts(maxDepth);
  if (skeleton == "seq") {
    counts = skeletons::Enum::Count<Empty, UTSNode, genChildren_func>::search(maxDepth, Empty(), root);
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
      ( "tree-type",
        boost::program_options::value<std::string>()->default_value("binomial"),
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

  hpx::register_startup_function(&workstealing::SearchManagerSched::registerPerformanceCounters);

  return hpx::init(desc_commandline, argc, argv);
}
