// N-Queens problem; following [1]
//
// [1] Richards, Martin (1997). Backtracking Algorithms in MCPL using
//     Bit Patterns and Recursion (Technical report).
//     University of Cambridge Computer Laboratory. UCAM-CL-TR-433.
//
// Based on code from Patrick Maier
//

#include <hpx/hpx_init.hpp>
#include <hpx/include/iostreams.hpp>

#include "YewPar.hpp"
#include "skeletons/Seq.hpp"
#include "skeletons/DepthBounded.hpp"
#include "skeletons/StackStealing.hpp"
#include "skeletons/Budget.hpp"

// N-queens doesn't have a space
struct Empty {};

struct Node {
    std::uint32_t all;
    std::uint32_t ld;
    std::uint32_t cols;
    std::uint32_t rd;
    std::uint32_t poss;

    Node() : all(0), ld(0), cols(0), rd(0), poss(0) {};
    Node(std::uint32_t all, std::uint32_t ld, std::uint32_t cols,
         std::uint32_t rd, std::uint32_t poss)
    : all(all), ld(ld), cols(cols), rd(rd), poss(poss) {};
};

namespace hpx { namespace serialization {
  template<class Archive>
  void serialize(Archive & ar, Node & x, const unsigned int version) {
    ar & x.all;
    ar & x.ld;
    ar & x.cols;
    ar & x.rd;
    ar & x.poss;
  }
}}

struct NodeGen : YewPar::NodeGenerator<Node, Empty> {
  std::uint32_t all;
  std::uint32_t poss;
  std::uint32_t ld;
  std::uint32_t cols;
  std::uint32_t rd;

  NodeGen(const Empty &, const Node & parent) :
  all(parent.all), ld(parent.ld), cols(parent.cols)
  , rd(parent.rd), poss(parent.poss) {
    this->numChildren = __builtin_popcount(poss);
  }

  Node next() override {
      auto bit = poss & -poss;
      poss -= bit;

      auto new_ld = (ld | bit) << 1;
      auto new_cols = cols | bit;
      auto new_rd = (rd | bit) >> 1;
      auto newP = ~(new_ld | new_cols | new_rd) & all;

      return Node (all, new_ld, new_cols, new_rd, newP);
  }
};

struct CountSols : YewPar::Enumerator<Node, std::uint64_t> {
  std::uint64_t count;
  CountSols() = default;

  void accumulate(const Node & n) override {
    if (n.cols == n.all) { count++; }
  }

  void combine(const std::uint64_t & other) override {
    count += other;
  }

  std::uint64_t get() override { return count; }
};

int hpx_main(boost::program_options::variables_map & opts) {
  auto spawnDepth = opts["spawn-depth"].as<unsigned>();
  auto size = opts["size"].as<unsigned>();
  auto skeleton   = opts["skeleton"].as<std::string>();

  auto all = (1 << size) - 1;
  Node root(all, 0, 0, 0, all);

  auto start_time = std::chrono::steady_clock::now();

  std::uint64_t count;
  if (skeleton == "seq") {
    YewPar::Skeletons::API::Params<> searchParameters;
    count = YewPar::Skeletons::Seq<NodeGen,
                                    YewPar::Skeletons::API::Enumeration,
                                    YewPar::Skeletons::API::Enumerator<CountSols>,
                                    YewPar::Skeletons::API::DepthLimited>
             ::search(Empty(), root, searchParameters);
  } else if (skeleton == "depthbounded") {
    YewPar::Skeletons::API::Params<> searchParameters;
    searchParameters.spawnDepth = spawnDepth;
    count = YewPar::Skeletons::DepthBounded<NodeGen,
                                              YewPar::Skeletons::API::Enumeration,
                                              YewPar::Skeletons::API::Enumerator<CountSols>,
                                              YewPar::Skeletons::API::DepthLimited>
             ::search(Empty(), root, searchParameters);
  } else if (skeleton == "stacksteal"){
    YewPar::Skeletons::API::Params<> searchParameters;
    searchParameters.stealAll = static_cast<bool>(opts.count("chunked"));
    count = YewPar::Skeletons::StackStealing<NodeGen,
                                              YewPar::Skeletons::API::Enumeration,
                                              YewPar::Skeletons::API::Enumerator<CountSols>,
                                              YewPar::Skeletons::API::DepthLimited>
             ::search(Empty(), root, searchParameters);
  } else if (skeleton == "budget"){
    YewPar::Skeletons::API::Params<> searchParameters;
    searchParameters.backtrackBudget = opts["backtrack-budget"].as<unsigned>();
    count = YewPar::Skeletons::Budget<NodeGen,
                                       YewPar::Skeletons::API::Enumeration,
                                       YewPar::Skeletons::API::Enumerator<CountSols>,
                                       YewPar::Skeletons::API::DepthLimited>
        ::search(Empty(), root, searchParameters);
  } else {
    hpx::cout << "Invalid skeleton type: " << skeleton << hpx::endl;
    return hpx::finalize();
  }

  auto overall_time = std::chrono::duration_cast<std::chrono::milliseconds>
                      (std::chrono::steady_clock::now() - start_time);

  hpx::cout << "Solution for n = " << size <<  ": " << count << hpx::endl;

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
    ( "size,n",
      boost::program_options::value<unsigned>()->default_value(8),
      "Boards size"
    )
    ( "backtrack-budget,b",
      boost::program_options::value<unsigned>()->default_value(500),
      "Number of backtracks before spawning work"
    )
    ( "verbose,v",
      boost::program_options::value<bool>()->default_value(false),
      "Enable verbose output"
    )
    ("chunked", "Use chunking with stack stealing");

  YewPar::registerPerformanceCounters();

  return hpx::init(desc_commandline, argc, argv);
}
