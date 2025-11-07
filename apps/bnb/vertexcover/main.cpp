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
#include <limits>


// Number of Words to use in our bitset representation
#ifndef NWORDS
#define NWORDS 8
#endif

// size of graph used in bounding 
static long long GRAPH_SIZE = 0;

// build dimacs graph
template<unsigned n_words_>
auto orderGraphFromFile(const dimacs::GraphFromFile & gf) -> std::pair<BitGraph<n_words_>, std::vector<std::pair<int,int>>> {
  const int n = gf.first;
  BitGraph<n_words_> g;
  g.resize(n);

  std::vector<std::pair<int,int>> edges;
  // set minimum capacity of the vector to the first member of the graph
  edges.reserve(n); 

  for (auto &kv : gf.second) {
    int u = kv.first;   
    for (int v : kv.second) {
      if (u < v) {
        g.add_edge(u, v);
        edges.emplace_back(u, v);
      }
    }
  }
  GRAPH_SIZE = g.size();
  return {g, edges};
}

// vertex cover solution state + serialization
struct VCSol {
  std::vector<int> vertices; 
  template <class Archive>
  void serialize(Archive & ar, const unsigned int) { 
    ar & vertices; 
  }
};
struct VCNode {
  friend class boost::serialization::access;

  VCSol sol;
  std::vector<std::pair<int,int>> uncoveredEdges; 
  int size = 0;                           

  long long getObj() const {
    if (!uncoveredEdges.empty())
      return std::numeric_limits<long long>::min() / 4; // non-solution
    return GRAPH_SIZE - size; // solution: |V| - |C|
  }

  template <class Archive>
  void serialize(Archive & ar, const unsigned int) {
    ar & sol;
    ar & uncoveredEdges;
    ar & size;
  }

  bool isSolution() const {
    return uncoveredEdges.empty();
  }
};

// bound: UB = |V| - ( |C| + ceil(m / delta) )
static long long vcBound(const BitGraph<NWORDS> & g, const VCNode & n) {
  const int m = (int)n.uncoveredEdges.size();
  if (m == 0) return GRAPH_SIZE - n.size;

  std::vector<int> deg(g.size(), 0);
  int Delta = 0;
  for (auto &e : n.uncoveredEdges) {
    int d1 = ++deg[e.first];
    int d2 = ++deg[e.second];
    if (d1 > Delta) Delta = d1;
    if (d2 > Delta) Delta = d2;
  }
  const int LB = (Delta == 0) ? 0 : ( (m + Delta - 1) / Delta ); // ceil(m / delta)
  return GRAPH_SIZE - (n.size + LB);
}

typedef func<decltype(&vcBound), &vcBound> vcBound_func;

// lazy node generation - branch on first uncovered edge (u,v)
struct VCGenNode : YewPar::NodeGenerator<VCNode, BitGraph<NWORDS>> {
  const BitGraph<NWORDS> &graph;
  VCNode parent;
  int next_child = 0;
  bool has_children = false;
  VCNode child0, child1; 

  VCGenNode(const BitGraph<NWORDS> & g, const VCNode & node) : graph(g), parent(node) {
    if (!parent.uncoveredEdges.empty()) {
      auto uv = parent.uncoveredEdges[0];
      int u = uv.first;
      int v = uv.second;

      child0 = make_child_add_vertex(u);
      child1 = make_child_add_vertex(v);

      this->numChildren = 2;
      has_children = true;
    } else {
      this->numChildren = 0;
      has_children = false;
      next_child = 0;
    }
  }

  static std::vector<std::pair<int,int>> remove_incident(const std::vector<std::pair<int,int>> &edges, int w) {
    std::vector<std::pair<int,int>> out;
    out.reserve(edges.size());
    for (auto &e : edges) {
      if (e.first != w && e.second != w) out.push_back(e);
    }
    return out;
  }

  VCNode make_child_add_vertex(int w) const {
    VCNode child = parent; // start from parent
    // IDs must be valid
    if (w < 0 || w >= (int)graph.size()) {
      return parent; // safe fallback
    }
    child.sol.vertices.push_back(w);
    child.size = parent.size + 1;
    child.uncoveredEdges = remove_incident(parent.uncoveredEdges, w);
    return child;
  }

  VCNode next() override {
    // serve prebuilt children up to numChildren
    if (!has_children || next_child >= this->numChildren) {
      return parent; 
    }
    VCNode out = (next_child == 0) ? child0 : child1;
    ++next_child;
    return out;
  }

};

// HPX main
int hpx_main(hpx::program_options::variables_map& opts) {
  const auto inputFile   = opts["input-file"].as<std::string>();

  // read in DIMACS graph file
  auto gf = dimacs::read_dimacs(inputFile);
  auto [graph, allEdges] = orderGraphFromFile<NWORDS>(gf);

  const auto spawnDepth  = opts["spawn-depth"].as<std::uint64_t>();
  const auto decisionK   = opts["decisionBound"].as<int>();

  auto start_time = std::chrono::steady_clock::now();
  
  // initialise root node and solution state
  VCSol vcsol; 
  VCNode root{ vcsol, allEdges, 0 };
  auto sol = root;
          
  const auto skeleton    = opts["skeleton"].as<std::string>();
  const auto backBudget  = opts["backtrack-budget"].as<unsigned>();
  const bool chunked     = static_cast<bool>(opts.count("chunked"));
  const auto poolType    = opts.count("poolType") ? opts["poolType"].as<std::string>() : std::string("depthpool");

  YewPar::Skeletons::API::Params<long long> P;
  if (decisionK != 0)  {
    P.expectedObjective = -decisionK; // maximise -|C| ≤ -K
  }

  if (skeleton == "seq") {
    if (decisionK != 0) {
      sol = YewPar::Skeletons::Seq<VCGenNode,
      YewPar::Skeletons::API::Decision,
      YewPar::Skeletons::API::BoundFunction<vcBound_func>,
      YewPar::Skeletons::API::PruneLevel>::search(graph, root, P);
    } 
    else {
      sol = YewPar::Skeletons::Seq<VCGenNode,
      YewPar::Skeletons::API::Optimisation,
      YewPar::Skeletons::API::BoundFunction<vcBound_func>,
      YewPar::Skeletons::API::PruneLevel>::search(graph, root);
    }
  } 
  else if (skeleton == "depthbounded") {
    P.spawnDepth = spawnDepth;
    if (decisionK != 0) {
      sol = YewPar::Skeletons::DepthBounded<VCGenNode,
      YewPar::Skeletons::API::Decision,
      YewPar::Skeletons::API::BoundFunction<vcBound_func>,
      YewPar::Skeletons::API::PruneLevel>::search(graph, root, P);
    } 
    else {
      if (poolType == "deque") {
        sol = YewPar::Skeletons::DepthBounded<VCGenNode,
        YewPar::Skeletons::API::Optimisation,
        YewPar::Skeletons::API::BoundFunction<vcBound_func>,
        YewPar::Skeletons::API::PruneLevel,
        YewPar::Skeletons::API::DepthBoundedPoolPolicy<
        Workstealing::Policies::Workpool>>::search(graph, root, P);
      } 
      else {
        sol = YewPar::Skeletons::DepthBounded<VCGenNode,
        YewPar::Skeletons::API::Optimisation,
        YewPar::Skeletons::API::BoundFunction<vcBound_func>,
        YewPar::Skeletons::API::PruneLevel,
        YewPar::Skeletons::API::DepthBoundedPoolPolicy<
        Workstealing::Policies::DepthPoolPolicy>>::search(graph, root, P);
      }
    }
  } 
  else if (skeleton == "stacksteal") {
    P.stealAll = chunked;
    if (decisionK != 0) {
      sol = YewPar::Skeletons::StackStealing<VCGenNode,
      YewPar::Skeletons::API::Decision,
      YewPar::Skeletons::API::BoundFunction<vcBound_func>,
      YewPar::Skeletons::API::PruneLevel>::search(graph, root, P);
    } 
    else {
      sol = YewPar::Skeletons::StackStealing<VCGenNode,
      YewPar::Skeletons::API::Optimisation,
      YewPar::Skeletons::API::BoundFunction<vcBound_func>,
      YewPar::Skeletons::API::PruneLevel>::search(graph, root, P);
    }
  } 
  else if (skeleton == "ordered") {
    P.spawnDepth = spawnDepth;
    if (opts.count("discrepancyOrder")) {
      sol = YewPar::Skeletons::Ordered<VCGenNode,
      YewPar::Skeletons::API::Optimisation,
      YewPar::Skeletons::API::BoundFunction<vcBound_func>,
      YewPar::Skeletons::API::DiscrepancySearch,
      YewPar::Skeletons::API::PruneLevel>::search(graph, root, P);
    } 
    else {
      sol = YewPar::Skeletons::Ordered<VCGenNode,
      YewPar::Skeletons::API::Optimisation,
      YewPar::Skeletons::API::BoundFunction<vcBound_func>,
      YewPar::Skeletons::API::PruneLevel>::search(graph, root, P);
    }
  } 
  else if (skeleton == "budget") {
    P.backtrackBudget = backBudget;
    if (decisionK != 0) {
      sol = YewPar::Skeletons::Budget<VCGenNode,
      YewPar::Skeletons::API::BoundFunction<vcBound_func>,
      YewPar::Skeletons::API::Decision,
      YewPar::Skeletons::API::PruneLevel>::search(graph, root, P);
    } 
    else {
      sol = YewPar::Skeletons::Budget<VCGenNode,
      YewPar::Skeletons::API::Optimisation,
      YewPar::Skeletons::API::BoundFunction<vcBound_func>,
      YewPar::Skeletons::API::PruneLevel>::search(graph, root, P);
    }
  } 
  else {
    hpx::cout << "Invalid skeleton type option. Use: seq, depthbounded, stacksteal, budget, ordered\n";
    hpx::finalize();
    return EXIT_FAILURE;
  }

  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);

  hpx::cout << "Minimum Vertex Cover Size = " << sol.size << "\n";
  hpx::cout << "Vertices: ";
  hpx::cout << "cpu = " << ms.count() << " ms\n";

  return hpx::finalize();
}

// main
int main(int argc, char* argv[]) {
  hpx::program_options::options_description
    desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

  desc_commandline.add_options()
    ("skeleton",
      hpx::program_options::value<std::string>()->default_value("seq"),
      "Which skeleton: seq, depthbounded, stacksteal, budget, ordered")
    ("spawn-depth,d",
      hpx::program_options::value<std::uint64_t>()->default_value(0),
      "Depth in the tree to spawn at")
    ("backtrack-budget,b",
      hpx::program_options::value<unsigned>()->default_value(50),
      "Backtracks before spawning work (budget skeleton)")
    ("input-file,f",
      hpx::program_options::value<std::string>()->required(),
      "DIMACS formatted input graph")
    ("discrepancyOrder", "Use discrepancy order with the ordered skeleton")
    ("chunked", "Use chunking with stack stealing")
    ("poolType",
      hpx::program_options::value<std::string>()->default_value("depthpool"),
      "Pool type for depthbounded skeleton: depthpool or deque")
    ("decisionBound",
      hpx::program_options::value<int>()->default_value(0),
      "Decision mode: search for a cover of size <= K")
  ;

  YewPar::registerPerformanceCounters();

  hpx::init_params args;
  args.desc_cmdline = desc_commandline;
  return hpx::init(argc, argv, args);
}
