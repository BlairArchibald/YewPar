#include <iostream>
#include <numeric>
#include <algorithm>
#include <vector>
#include <map>
#include <chrono>

#include <hpx/hpx_init.hpp>
#include <hpx/iostream.hpp>

#include <boost/serialization/access.hpp>

#include "../maxclique/BitGraph.hpp"
#include "../maxclique/BitSet.hpp"
#include "../maxclique/DimacsParser.hpp"

#include "YewPar.hpp"

#include "skeletons/Seq.hpp"
#include "skeletons/DepthBounded.hpp"
#include "skeletons/StackStealing.hpp"
#include "skeletons/Ordered.hpp"
#include "skeletons/Budget.hpp"

#include "util/func.hpp"
#include "util/NodeGenerator.hpp"

// Number of Words to use in our bitset representation
#ifndef NWORDS
#define NWORDS 8
#endif

// graph production
template<unsigned n_words_>
auto orderGraphFromFile(const dimacs::GraphFromFile & graph, std::map<int, int> & inverseMap) -> BitGraph<n_words_> {
    std::vector<int> order(graph.first);
    std::iota(order.begin(), order.end(), 0);

    // Order by degree, tie break on number
    std::vector<int> degrees;
    std::transform(order.begin(), order.end(), std::back_inserter(degrees),
                   [&] (int v) { return graph.second.find(v)->second.size(); });
    
    std::sort(order.begin(), order.end(),
              [&] (int a, int b) { return ! (degrees[a] < degrees[b] || (degrees[a] == degrees[b] && a > b)); });

    // Construct a new graph with this new ordering
    BitGraph<n_words_> g;
    g.resize(graph.first);  

    for (unsigned i = 0 ; i < graph.first ; ++i) {
      for (unsigned j = 0 ; j < graph.first ; ++j) {
        if (graph.second.find(order[i])->second.count(order[j])) {
          g.add_edge(i, j);
        } 
      }
    }
      
    // Create the inverse map
    for (unsigned i = 0 ; i < order.size() ; ++i) {
      inverseMap[order[i]] = i;
    }
    return g;
}

// vertec cover solution state

// node generator

// bounding function(s)

// hpx main
int hpx_main(hpx::program_options::variables_map& opts) {
  auto inputFile = opts["input-file"].as<std::string>();
  auto graphFile = dimacs::read_dimacs(inputFile);

  std::map<int, int> inverseMap;
  auto graph = orderGraphFromFile<NWORDS>(graphFile, inverseMap);



  return hpx::finalize();
}
// main program
int main(int argc, char* argv[]) {
  hpx::program_options::options_description
  desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

  desc_commandline.add_options()
  ("skeleton", 
      hpx::program_options::value<std::string>()->default_value("seq"),
      "Skeleton type"
  )
  ("spawn-depth,d", 
      hpx::program_options::value<std::uint64_t>()->default_value(0),
      "Depth in the tree to spawn at"
  )
  ("backtrack-budget,b", 
      hpx::program_options::value<unsigned>()->default_value(50),
      "Number of backtracks before spawning work")
  ("input-file,f", 
      hpx::program_options::value<std::string>()->required(),
      "DIMACS input file"
  )
  ("decisionBound", 
      hpx::program_options::value<int>()->default_value(0),
      "Decision mode: stop at this bound"
  );

  YewPar::registerPerformanceCounters();
  hpx::init_params args;
  args.desc_cmdline = desc_commandline;
  return hpx::init(argc, argv, args);
}