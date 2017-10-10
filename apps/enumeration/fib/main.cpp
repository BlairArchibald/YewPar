#include <hpx/hpx.hpp>
#include <hpx/hpx_init.hpp>

#include <vector>
#include <memory>
#include <chrono>

#include "enumerate/skeletons.hpp"
#include "enumerate/dist-nodegen-stack.hpp"
#include "util/func.hpp"

// Fib doesn't have a space
struct Empty {};


struct NodeGen : skeletons::Enum::NodeGenerator<Empty, std::uint64_t> {
  std::uint64_t n;
  unsigned i = 1;

  NodeGen() : n(0) {
    this->numChildren = 0;
  }

  NodeGen(const std::uint64_t & n) : n(n) {
    this->numChildren = 2;
  }

  std::uint64_t next() override {
    auto res = n - i;
    ++i;
    return res;
  }
};

NodeGen generateChildren(const Empty & space, const std::uint64_t & n) {
  return NodeGen(n);
}

#define MAX_DEPTH 50

typedef func<decltype(&generateChildren), &generateChildren> genChildren_func;
REGISTER_ENUM_REGISTRY(Empty, std::uint64_t)

using cFunc = skeletons::Enum::DistCount<Empty, std::uint64_t, genChildren_func, skeletons::Enum::StackOfNodes, std::integral_constant<std::size_t, MAX_DEPTH> >::ChildTask;
using solType = std::uint64_t;
REGISTER_SEARCHMANAGER(solType, cFunc)

int hpx_main(boost::program_options::variables_map & opts) {
  auto spawnDepth = opts["spawn-depth"].as<unsigned>();
  auto maxDepth   = opts["until-depth"].as<unsigned>();
  auto skeleton   = opts["skeleton-type"].as<std::string>();

  auto start_time = std::chrono::steady_clock::now();

  std::vector<std::uint64_t> counts(maxDepth);
  if (skeleton == "seq") {
    counts = skeletons::Enum::Count<Empty, std::uint64_t, genChildren_func>::search(maxDepth, Empty(), maxDepth - 1);
  } else if (skeleton == "genstack"){
    counts = skeletons::Enum::DistCount<Empty, std::uint64_t, genChildren_func, skeletons::Enum::StackOfNodes, std::integral_constant<std::size_t, MAX_DEPTH> >::count(maxDepth, Empty(), maxDepth - 1);
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

  hpx::register_startup_function(&workstealing::SearchManagerSched::registerPerformanceCounters);

  return hpx::init(desc_commandline, argc, argv);
}
