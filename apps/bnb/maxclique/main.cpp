#include <iostream>
#include <numeric>
#include <algorithm>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <typeinfo>

#include <hpx/hpx_init.hpp>
#include <hpx/include/iostreams.hpp>

#include <boost/serialization/access.hpp>

#include "DimacsParser.hpp"
#include "BitGraph.hpp"
#include "BitSet.hpp"

#include "YewPar.hpp"

#include "skeletons/Seq.hpp"
#include "skeletons/DepthBounded.hpp"
#include "skeletons/StackStealing.hpp"
#include "skeletons/Ordered.hpp"
#include "skeletons/Budget.hpp"

#include "util/func.hpp"
#include "util/NodeGenerator.hpp"

// Number of Words to use in our bitset representation
// Possible to specify at compile to to handler bigger graphs if required
#ifndef NWORDS
#define NWORDS 8
#endif

// Order a graphFromFile and return an ordered graph alongside a map to invert
// the vertex numbering at the end.
template<unsigned n_words_>
auto orderGraphFromFile(const dimacs::GraphFromFile & g, std::map<int,int> & inv) -> BitGraph<n_words_> {
  std::vector<int> order(g.first);
  std::iota(order.begin(), order.end(), 0);

  // Order by degree, tie break on number
  std::vector<int> degrees;
  std::transform(order.begin(), order.end(), std::back_inserter(degrees),
                 [&] (int v) { return g.second.find(v)->second.size(); });

  std::sort(order.begin(), order.end(),
            [&] (int a, int b) { return ! (degrees[a] < degrees[b] || (degrees[a] == degrees[b] && a > b)); });


  // Construct a new graph with this new ordering
  BitGraph<n_words_> graph;
  graph.resize(g.first);

  for (unsigned i = 0 ; i < g.first ; ++i)
    for (unsigned j = 0 ; j < g.first ; ++j)
      if (g.second.find(order[i])->second.count(order[j]))
        graph.add_edge(i, j);

  // Create inv map (maybe just return order?)
  for (int i = 0; i < order.size(); i++) {
    inv[i] = order[i];
  }

  return graph;
}

template<unsigned n_words_>
auto colour_class_order(const BitGraph<n_words_> & graph,
                        const BitSet<n_words_> & p,
                        std::array<unsigned, n_words_ * bits_per_word> & p_order,
                        std::array<unsigned, n_words_ * bits_per_word> & p_bounds) -> void {
  BitSet<n_words_> p_left = p; // not coloured yet
  unsigned colour = 0;         // current colour
  unsigned i = 0;              // position in p_bounds

  // while we've things left to colour
  while (! p_left.empty()) {
    // next colour
    ++colour;
    // things that can still be given this colour
    BitSet<n_words_> q = p_left;

    // while we can still give something this colour
    while (! q.empty()) {
      // first thing we can colour
      int v = q.first_set_bit();
      p_left.unset(v);
      q.unset(v);

      // can't give anything adjacent to this the same colour
      graph.intersect_with_row_complement(v, q);

      // record in result
      p_bounds[i] = colour;
      p_order[i] = v;
      ++i;
    }
  }
}

// Main Maxclique B&B Functions
// Probably needs a copy constructor
struct MCSol {
  std::vector<int> members;
  int colours;

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & members;
    ar & colours;
  }
};

struct MCNode {
  friend class boost::serialization::access;

  MCSol sol;
  int size;
  BitSet<NWORDS> remaining;

  int getObj() const {
    return size;
  }

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & sol;
    ar & size;
    ar & remaining;
  }

};

struct GenNode : YewPar::NodeGenerator<MCNode, BitGraph<NWORDS> > {
  std::array<unsigned, NWORDS * bits_per_word> p_order;
  std::array<unsigned, NWORDS * bits_per_word> colourClass;

  std::reference_wrapper<const BitGraph<NWORDS> > graph;

  MCSol childSol;
  int childBnd;
  BitSet<NWORDS> p;

  int v;

  GenNode(const BitGraph<NWORDS> & graph, const MCNode & n) : graph(std::cref(graph)) {
    colour_class_order(graph, n.remaining, p_order, colourClass);
    childSol = n.sol;
    childBnd = n.size + 1;
    p = n.remaining;
    numChildren = p.popcount();
    v = numChildren - 1;
  }

  // Get the next value
  MCNode next() override {
    auto sol = childSol;
    sol.members.push_back(p_order[v]);
    sol.colours = colourClass[v] - 1;

    auto cands = p;
    graph.get().intersect_with_row(p_order[v], cands);

    // Side effectful function update
    p.unset(p_order[v]);
    v--;

    return {sol, childBnd, cands};
  }

  MCNode nth(unsigned n) {
    auto pos = v - n;

    auto sol = childSol;
    sol.members.push_back(p_order[pos]);
    sol.colours = colourClass[pos] - 1;

    auto cands = p;
    // Remove all choices from the left "left" of the one we care about
    for (auto i = v; i > pos; --i) {
      cands.unset(p_order[i]);
    }

    graph.get().intersect_with_row(p_order[pos], cands);

    return {sol, childBnd, cands};
  }
};

int upperBound(const BitGraph<NWORDS> & space, const MCNode & n) {
  return n.size + n.sol.colours;
}

typedef func<decltype(&upperBound), &upperBound> upperBound_func;

int hpx_main(boost::program_options::variables_map & opts) {
  /*
  if (!opts.count("input-file")) {
    std::cerr << "You must provide an DIMACS input file with \n";
    hpx::finalize();
    return EXIT_FAILURE;
  }
  */

  //boost::program_options::notify(opts);

  auto inputFile = opts["input-file"].as<std::string>();

  auto gFile = dimacs::read_dimacs(inputFile);

  // Order the graph (keep a hold of the map)
  std::map<int, int> invMap;
  auto graph = orderGraphFromFile<NWORDS>(gFile, invMap);

  auto spawnDepth = opts["spawn-depth"].as<std::uint64_t>();
  auto decisionBound = opts["decisionBound"].as<int>();

  auto start_time = std::chrono::steady_clock::now();

  // Initialise Root Node
  MCSol mcsol;
  mcsol.members.reserve(graph.size());
  mcsol.colours = 0;

  BitSet<NWORDS> cands;
  cands.resize(graph.size());
  cands.set_all();
  MCNode root = { mcsol, 0, cands };

  auto sol = root;
  auto skeletonType = opts["skeleton"].as<std::string>();

  if (skeletonType == "depthbounded") {
    if (decisionBound != 0) {
      YewPar::Skeletons::API::Params<int> searchParameters;
      searchParameters.expectedObjective = decisionBound;
      searchParameters.spawnDepth = spawnDepth;
      sol = YewPar::Skeletons::DepthBounded<GenNode,
                                           YewPar::Skeletons::API::Decision,
                                           YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                           YewPar::Skeletons::API::PruneLevel>
            ::search(graph, root, searchParameters);
    } else {
      YewPar::Skeletons::API::Params<int> searchParameters;
      searchParameters.spawnDepth = spawnDepth;
      auto poolType = opts["poolType"].as<std::string>();
      // EXTENSION
      if (opts.count("nodes")) {
          sol = YewPar::Skeletons::DepthBounded<GenNode,
                                               YewPar::Skeletons::API::Optimisation,
                                               YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                               YewPar::Skeletons::API::PruneLevel,
                                               YewPar::Skeletons::API::NodeCounts,
                                               YewPar::Skeletons::API::DepthBoundedPoolPolicy<
                                                 Workstealing::Policies::DepthPoolPolicy> >
              ::search(graph, root, searchParameters);
      } else if (opts.count("backtracks")) {
          sol = YewPar::Skeletons::DepthBounded<GenNode,
                                               YewPar::Skeletons::API::Optimisation,
                                               YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                               YewPar::Skeletons::API::PruneLevel,
                                               YewPar::Skeletons::API::Backtracks,
                                               YewPar::Skeletons::API::DepthBoundedPoolPolicy<
                                                 Workstealing::Policies::DepthPoolPolicy> >
              ::search(graph, root, searchParameters);
      } else if (opts.count("regularity")) {
          sol = YewPar::Skeletons::DepthBounded<GenNode,
                                               YewPar::Skeletons::API::Optimisation,
                                               YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                               YewPar::Skeletons::API::PruneLevel,
                                               YewPar::Skeletons::API::Regularity,
                                               YewPar::Skeletons::API::DepthBoundedPoolPolicy<
                                                 Workstealing::Policies::DepthPoolPolicy> >
              ::search(graph, root, searchParameters);
      } else {
        sol = YewPar::Skeletons::DepthBounded<GenNode,
                                               YewPar::Skeletons::API::Optimisation,
                                               YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                               YewPar::Skeletons::API::PruneLevel,
                                               YewPar::Skeletons::API::DepthBoundedPoolPolicy<
                                               Workstealing::Policies::DepthPoolPolicy> >
              ::search(graph, root, searchParameters);
      }
    }
  } else if (skeletonType == "stacksteal") {
    if (decisionBound != 0) {
      YewPar::Skeletons::API::Params<int> searchParameters;
      searchParameters.expectedObjective = decisionBound;
      searchParameters.stealAll = static_cast<bool>(opts.count("chunked"));
      sol = YewPar::Skeletons::StackStealing<GenNode,
                                             YewPar::Skeletons::API::Decision,
                                             YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                             YewPar::Skeletons::API::PruneLevel>
          ::search(graph, root, searchParameters);
    } else {
      YewPar::Skeletons::API::Params<int> searchParameters;
      searchParameters.stealAll = static_cast<bool>(opts.count("chunked"));
      if (opts.count("nodes")) {
        sol = YewPar::Skeletons::StackStealing<GenNode,
                                              YewPar::Skeletons::API::Optimisation,
                                              YewPar::Skeletons::API::NodeCounts,
                                              YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                              YewPar::Skeletons::API::PruneLevel>
            ::search(graph, root, searchParameters);
      } else if (opts.count("backtracks"))  {
        sol = YewPar::Skeletons::StackStealing<GenNode,
                                              YewPar::Skeletons::API::Optimisation,
                                              YewPar::Skeletons::API::Backtracks,
                                              YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                              YewPar::Skeletons::API::PruneLevel>
            ::search(graph, root, searchParameters);
      } else if (opts.count("regularity")) {
        sol = YewPar::Skeletons::StackStealing<GenNode,
                                              YewPar::Skeletons::API::Optimisation,
                                              YewPar::Skeletons::API::Regularity,
                                              YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                              YewPar::Skeletons::API::PruneLevel>
            ::search(graph, root, searchParameters);
      } else {
        sol = YewPar::Skeletons::StackStealing<GenNode,
                                              YewPar::Skeletons::API::Optimisation,
                                              YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                              YewPar::Skeletons::API::PruneLevel>
            ::search(graph, root, searchParameters);
      }
    }
  } else if (skeletonType == "budget") {
    if (decisionBound != 0) {
      YewPar::Skeletons::API::Params<int> searchParameters;
      searchParameters.backtrackBudget = opts["backtrack-budget"].as<unsigned>();
      searchParameters.expectedObjective = decisionBound;
      sol = YewPar::Skeletons::Budget<GenNode,
                                      YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                      YewPar::Skeletons::API::Decision,
                                      YewPar::Skeletons::API::PruneLevel>
          ::search(graph, root, searchParameters);
    } else {
      YewPar::Skeletons::API::Params<int> searchParameters;
      searchParameters.backtrackBudget = opts["backtrack-budget"].as<unsigned>();
      if (opts.count("nodes")) {
        sol = YewPar::Skeletons::Budget<GenNode,
                                        YewPar::Skeletons::API::Optimisation,
                                        YewPar::Skeletons::API::NodeCounts,
                                        YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                        YewPar::Skeletons::API::PruneLevel>
          ::search(graph, root, searchParameters);
      } else if(opts.count("backtracks")) {
        sol = YewPar::Skeletons::Budget<GenNode,
                                        YewPar::Skeletons::API::Optimisation,
                                        YewPar::Skeletons::API::Backtracks,
                                        YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                        YewPar::Skeletons::API::PruneLevel>
          ::search(graph, root, searchParameters);
      } else if (opts.count("regularity")) {
        sol = YewPar::Skeletons::Budget<GenNode,
                                        YewPar::Skeletons::API::Optimisation,
                                        YewPar::Skeletons::API::Regularity,
                                        YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                        YewPar::Skeletons::API::PruneLevel>
          ::search(graph, root, searchParameters);
      } else {
        sol = YewPar::Skeletons::Budget<GenNode,
                                        YewPar::Skeletons::API::Optimisation,
                                        YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                        YewPar::Skeletons::API::PruneLevel>
          ::search(graph, root, searchParameters);
      }
    }
  } else {
    hpx::cout << "Invalid skeleton type option. Should be: seq, depthbound, stacksteal or ordered" << hpx::endl;
    hpx::finalize();
    return EXIT_FAILURE;
  }

  auto overall_time = std::chrono::duration_cast<std::chrono::milliseconds>
    (std::chrono::steady_clock::now() - start_time);

  hpx::cout << "MaxClique Size = " << sol.size << hpx::endl;
  hpx::cout << "cpu = " << overall_time.count() << hpx::endl;

  return hpx::finalize();
}

int main (int argc, char* argv[]) {
  boost::program_options::options_description
    desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

  desc_commandline.add_options()
    ( "skeleton",
      boost::program_options::value<std::string>()->default_value("seq"),
      "Which skeleton to use: seq, depthbound, stacksteal, budget, or ordered"
      )
    ( "spawn-depth,d",
      boost::program_options::value<std::uint64_t>()->default_value(0),
      "Depth in the tree to spawn at"
      )
    ( "backtrack-budget,b",
      boost::program_options::value<unsigned>()->default_value(50),
      "Number of backtracks before spawning work"
      )
    ( "input-file,f",
      boost::program_options::value<std::string>()->required(),
      "DIMACS formatted input graph"
      )
    ("discrepancyOrder", "Use discrepancy order for the ordered skeleton")
    ("chunked", "Use chunking with stack stealing")
    ("poolType",
     boost::program_options::value<std::string>()->default_value("depthpool"),
     "Pool type for depthbounded skeleton")
    ( "decisionBound",
    boost::program_options::value<int>()->default_value(0),
    "For Decision Skeletons. Size of the clique to search for"
    )
    ("backtracks", "Collect the backtracks metric")
    ("nodes", "Collect the backtracks metric")
    ("regularity", "Collect the backtracks metric");

  YewPar::registerPerformanceCounters();

  return hpx::init(desc_commandline, argc, argv);
}
