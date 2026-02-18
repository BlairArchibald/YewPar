#include <iostream>
#include <numeric>
#include <algorithm>
#include <vector>
#include <chrono>
#include <limits>
#include <string>

#include <hpx/hpx_init.hpp>
#include <hpx/iostream.hpp>

#include <boost/serialization/access.hpp>

#include "../maxclique/DimacsParser.hpp"
#include "../maxclique/BitGraph.hpp"
#include "../maxclique/BitSet.hpp"

#include "YewPar.hpp"

#include "skeletons/Seq.hpp"
#include "skeletons/DepthBounded.hpp"
#include "skeletons/StackStealing.hpp"
#include "skeletons/Ordered.hpp"
#include "skeletons/Budget.hpp"

#include "util/func.hpp"
#include "util/NodeGenerator.hpp"
#include "workstealing/policies/SearchManager.hpp"

#ifndef NWORDS
#define NWORDS 16
#endif

// Build BitGraph from DIMACS and return the COMPLEMENT graph
template<unsigned n_words_>
BitGraph<n_words_> buildGraphFromFile(const dimacs::GraphFromFile &g) {
    int N = g.first;

    // Build original graph (clique instance)
    BitGraph<n_words_> original;
    original.resize(N);

    for (auto &kv : g.second) {
        int u = kv.first;
        for (int v : kv.second) {
            if (u != v) {
                original.add_edge(u, v);
            }
        }
    }

    // Build complement graph
    BitGraph<n_words_> comp;
    comp.resize(N);

    for (int u = 0; u < N; ++u) {
        for (int v = u + 1; v < N; ++v) {
            if (!original.adjacent(u, v)) {
                comp.add_edge(u, v);
            }
        }
    }
    return comp;
}

// vertex cover node definition
struct VCNode {
  friend class boost::serialization::access;

  int size = 0;
  BitSet<NWORDS> inCover;
  BitSet<NWORDS> active;
  bool isCover = false;

  // function for optimization skeleton - return current cover size if valid cover, otherwise +inf
  int getObj() const {
    if (!isCover) return std::numeric_limits<int>::max();
    return size;
  }

  // serialization support for multi-node work stealing
  template <class Archive>
  void serialize(Archive &ar, const unsigned int) {
    ar & size;
    ar & inCover;
    ar & active;
    ar & isCover;
  }
};

// helper function to get vertices that have been generated in the graph and not yet included in the cover
static inline BitSet<NWORDS> undecided_vertices(const VCNode &n) {
  BitSet<NWORDS> u = n.active;
  u.intersect_with_complement(n.inCover);
  return u;
}

// check if all remaining active vertices are covered by the current inCover set
static bool check_is_cover(const BitGraph<NWORDS> &g, const VCNode &n) {
  int N = g.size();
  BitSet<NWORDS> rest = undecided_vertices(n);

  for (int u = 0; u < N; ++u) {
    if (!rest.test(u)) continue;
    BitSet<NWORDS> nbrs = rest;
    g.intersect_with_row(u, nbrs);
    if (!nbrs.empty()) return false; // found uncovered edge
  }
  return true;
}

// reduction rule: remove degree-0 vertices in the undecided induced subgraph
// removes all isolated vertices and marks node as cover if all vertices are covered
static bool apply_reductions(const BitGraph<NWORDS> &g, VCNode &n) {
  bool changed = false;
  int N = g.size();

  while (true) {
    bool localChange = false;
    BitSet<NWORDS> undec = undecided_vertices(n);
    for (int u = 0; u < N; ++u) {
      if (!undec.test(u)) continue;

      BitSet<NWORDS> nbrs = undec;
      g.intersect_with_row(u, nbrs); // neighbors among undecided vertices

      if (nbrs.empty()) {
        n.active.unset(u);
        undec.unset(u);
        localChange = true;
      }
    }
    changed |= localChange;
    if (!localChange) break;
  }
  n.isCover = check_is_cover(g, n);
  return changed;
}

// Matching-based lower bound on remaining cover size
// LB = |C| + size of greedy maximal matching on undecided induced subgraph
int vcBound(const BitGraph<NWORDS> &g, const VCNode &n) {
  int N = g.size();

  BitSet<NWORDS> avail = undecided_vertices(n);
  BitSet<NWORDS> used;
  BitSet<NWORDS> nbrs;
  
  used.resize(N);
  used.reset_all();
  nbrs.resize(N);

  int matchingSize = 0;

  for (int u = 0; u < N; ++u) {
    if (!avail.test(u) || used.test(u)) continue;

    nbrs = avail;
    g.intersect_with_row(u, nbrs);
    nbrs.intersect_with_complement(used);

    int v = nbrs.first_set_bit();
    if (v != -1) {
      used.set(u);
      used.set(v);
      avail.unset(u);
      avail.unset(v);
      ++matchingSize;
    } else {
      avail.unset(u);
    }
  }
  return n.size + matchingSize;
}

typedef func<decltype(&vcBound), &vcBound> vcBound_func;

// NodeGenerator: pick a high-degree vertex and branch on include / exclude
struct VCGenNode : YewPar::NodeGenerator<VCNode, BitGraph<NWORDS>> {
  std::reference_wrapper<const BitGraph<NWORDS>> graph;
  VCNode parent;
  int next_child = 0;
  int branchVertex = -1;

  // Default destructor for deserialization
  VCGenNode() : graph(std::cref(*(const BitGraph<NWORDS>*)nullptr)) {}

  // Serialization support for multi-node work stealing
  // Note: graph reference is NOT serialized - it's shared read-only data
  // that must be reconstructed from the registry on the receiving node
  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive &ar, const unsigned int) {
    ar & parent;
    ar & next_child;
    ar & branchVertex;
    ar & numChildren;
  }

  // constructor for initial node generator
  VCGenNode(const BitGraph<NWORDS> &g, const VCNode &node)
      : graph(std::cref(g)), parent(node), next_child(0), branchVertex(-1) {

    if (parent.isCover || parent.active.empty()) {
      numChildren = 0;
      return;
    }

    const auto &graphRef = getGraph();
    int N = graphRef.size();

    BitSet<NWORDS> undec = parent.active;
    undec.intersect_with_complement(parent.inCover);

    int bestDeg = -1;
    int bestV = -1;
    BitSet<NWORDS> nbrs;
    nbrs.resize(N);

    // for each undecided vertex, compute degree of subgraph, undecided vertices, and pick vertex with highest degree to branch on
    for (int u = 0; u < N; ++u) {
      if (!undec.test(u)) continue;

      nbrs = undec;
      graphRef.intersect_with_row(u, nbrs);

      int d = static_cast<int>(nbrs.popcount());
      if (d > bestDeg) {
        bestDeg = d;
        bestV = u;
      }
    }

    if (bestV == -1) {
      parent.isCover = check_is_cover(graphRef, parent);
      numChildren = 0;
      return;
    }
    branchVertex = bestV;
    numChildren = 2;
  }

  // helper function to include current vertex in cover and apply reductions
  VCNode include_vertex(int v) const {
    VCNode child = parent;
    if (!child.inCover.test(v)) {
      child.inCover.set(v);
      child.size += 1;
    }
    // get graph from registry if reference is invalid (after deserialization)
    const auto &g = getGraph();
    apply_reductions(g, child);
    return child;
  }

  // helper function to exclude current vertex from cover, add neighbors to cover, and apply reductions
  VCNode exclude_vertex(int v) const {
    VCNode child = parent;
    // get graph from registry if reference is invalid (after deserialization)
    const auto &g = getGraph();
    int N = g.size();

    BitSet<NWORDS> nbrs = child.active;
    g.intersect_with_row(v, nbrs); // neighbors among active vertices

    for (int u = 0; u < N; ++u) {
      if (!nbrs.test(u)) continue;
      if (!child.inCover.test(u)) {
        child.inCover.set(u);
        child.size += 1;
      }
    }

    child.active.unset(v);
    apply_reductions(g, child);
    return child;
  }

  // generate next child node by including or excluding the branch vertex
  VCNode next() override {
    if (next_child >= numChildren) {
      return parent; // unused
    }
    VCNode out = (next_child == 0) ? include_vertex(branchVertex) : exclude_vertex(branchVertex);
    ++next_child;
    return out;
  }

  // helper to get graph reference, either from member reference or from registry if deserialized
  private:
    // helper to get graph - uses reference if valid, otherwise gets from registry
    const BitGraph<NWORDS>& getGraph() const {
      // after deserialization on remote node, need to get from registry
      if (&graph.get() == nullptr) {
        return YewPar::Registry<BitGraph<NWORDS>, VCNode, int, YewPar::CountNodesEnumerator<VCNode>>::gReg->space;
      }
      return graph.get();
    }
};

// HPX main function to set up problem and call skeleton search
int hpx_main(hpx::program_options::variables_map &opts) {
  auto inputFile = opts["input-file"].as<std::string>();
  auto gFile = dimacs::read_dimacs(inputFile);
  auto graph = buildGraphFromFile<NWORDS>(gFile);

  auto start_time = std::chrono::steady_clock::now();

  VCNode root;
  root.size = 0;
  root.inCover.resize(graph.size());
  root.inCover.reset_all();
  root.active.resize(graph.size());
  root.active.set_all();
  root.isCover = check_is_cover(graph, root);

  apply_reductions(graph, root);

  VCNode sol = root;

  YewPar::Skeletons::API::Params<int> P;
  P.initialBound = graph.size();

  auto skeletonType = opts["skeleton"].as<std::string>();
  
  // Debug: Print configuration
  if (hpx::get_locality_id() == 0) {
    hpx::cout << "=== Configuration ===\n";
    hpx::cout << "Skeleton: " << skeletonType << "\n";
    hpx::cout << "Graph size: " << graph.size() << " vertices\n";
    hpx::cout << "Num localities: " << hpx::get_num_localities(hpx::launch::sync) << "\n";
    hpx::cout << "HPX threads per locality: " << hpx::get_os_thread_count() << "\n";
  }
  
  if (skeletonType == "seq") {
    sol = YewPar::Skeletons::Seq<VCGenNode,
          YewPar::Skeletons::API::Optimisation,
          YewPar::Skeletons::API::BoundFunction<vcBound_func>,
          YewPar::Skeletons::API::ObjectiveComparison<std::less<int>>>
          ::search(graph, root, P);
  }
  else if (skeletonType == "depthbounded") {
    P.spawnDepth = opts["spawn-depth"].as<std::uint64_t>();
    if (hpx::get_locality_id() == 0) {
      hpx::cout << "Spawn depth: " << P.spawnDepth << "\n";
      hpx::cout << "====================\n" << hpx::flush;
    }
    sol = YewPar::Skeletons::DepthBounded<VCGenNode,
          YewPar::Skeletons::API::Optimisation,
          YewPar::Skeletons::API::BoundFunction<vcBound_func>,
          YewPar::Skeletons::API::ObjectiveComparison<std::less<int>>>
          ::search(graph, root, P);
  }
  else if (skeletonType == "stacksteal") {
    P.stealAll = static_cast<bool>(opts.count("chunked"));
    if (hpx::get_locality_id() == 0) {
      hpx::cout << "Chunked: " << (P.stealAll ? "true" : "false") << "\n";
      hpx::cout << "====================\n" << hpx::flush;
    }
    sol = YewPar::Skeletons::StackStealing<VCGenNode,
          YewPar::Skeletons::API::Optimisation,
          YewPar::Skeletons::API::BoundFunction<vcBound_func>,
          YewPar::Skeletons::API::ObjectiveComparison<std::less<int>>>
          ::search(graph, root, P);
  }
  else if (skeletonType == "ordered") {
    P.spawnDepth = opts["spawn-depth"].as<std::uint64_t>();
    if (opts.count("discrepancyOrder")) {
      sol = YewPar::Skeletons::Ordered<VCGenNode,
            YewPar::Skeletons::API::Optimisation,
            YewPar::Skeletons::API::BoundFunction<vcBound_func>,
            YewPar::Skeletons::API::ObjectiveComparison<std::less<int>>,
            YewPar::Skeletons::API::DiscrepancySearch>
            ::search(graph, root, P);
    } else {
      sol = YewPar::Skeletons::Ordered<VCGenNode,
            YewPar::Skeletons::API::Optimisation,
            YewPar::Skeletons::API::BoundFunction<vcBound_func>,
            YewPar::Skeletons::API::ObjectiveComparison<std::less<int>>>
            ::search(graph, root, P);
    }
  }
  else if (skeletonType == "budget") {
    P.backtrackBudget = opts["backtrack-budget"].as<unsigned>();
    if (hpx::get_locality_id() == 0) {
      hpx::cout << "Backtrack budget: " << P.backtrackBudget << "\n";
      hpx::cout << "====================\n" << hpx::flush;
    }
    sol = YewPar::Skeletons::Budget<VCGenNode,
          YewPar::Skeletons::API::Optimisation,
          YewPar::Skeletons::API::BoundFunction<vcBound_func>,
          YewPar::Skeletons::API::ObjectiveComparison<std::less<int>>>
          ::search(graph, root, P);
  }
  else {
    hpx::cout << "Invalid skeleton type\n";
    return hpx::finalize();
  }

  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start_time);

  hpx::cout << "Minimum Vertex Cover Size = " << sol.size << "\n";
  hpx::cout << "cpu = " << ms.count() << " ms\n";

  // Print task distribution statistics for parallel skeletons
  // IMPORTANT: Print from ALL localities to see work distribution
  if (skeletonType != "seq") {
    // Force barrier to ensure all localities print before exit
    hpx::wait_all();
    
    auto localId = hpx::get_locality_id();
    auto numLocs = hpx::get_num_localities(hpx::launch::sync);
    
    // Each locality prints its stats
    hpx::cout << "\n=== Task Distribution Statistics (Locality " << localId << " of " << numLocs << ") ===\n";
    hpx::cout << "Local Steals: " << Workstealing::Policies::SearchManagerPerf::perf_localSteals.load() << "\n";
    hpx::cout << "Distributed Steals: " << Workstealing::Policies::SearchManagerPerf::perf_distributedSteals.load() << "\n";
    hpx::cout << "Failed Local Steals: " << Workstealing::Policies::SearchManagerPerf::perf_failedLocalSteals.load() << "\n";
    hpx::cout << "Failed Distributed Steals: " << Workstealing::Policies::SearchManagerPerf::perf_failedDistributedSteals.load() << "\n";
    
    auto totalSteals = Workstealing::Policies::SearchManagerPerf::perf_localSteals.load() + 
                       Workstealing::Policies::SearchManagerPerf::perf_distributedSteals.load();
    auto totalFailed = Workstealing::Policies::SearchManagerPerf::perf_failedLocalSteals.load() + 
                       Workstealing::Policies::SearchManagerPerf::perf_failedDistributedSteals.load();
    
    if (totalSteals == 0 && totalFailed == 0) {
      hpx::cout << "*** WARNING: This locality did NO stealing attempts - likely idle! ***\n";
    }
    
    if (Workstealing::Policies::SearchManagerPerf::perf_distributedSteals.load() > 0 || 
        !Workstealing::Policies::SearchManagerPerf::distributedStealsList.empty()) {
      hpx::cout << "\n=== Distributed Steal Details ===\n";
      Workstealing::Policies::SearchManagerPerf::printDistributedStealsList();
    }
    
    if (!Workstealing::Policies::SearchManagerPerf::chunkSizeList.empty()) {
      hpx::cout << "\n=== Chunk Sizes ===\n";
      Workstealing::Policies::SearchManagerPerf::printChunkSizeList();
    }
    
    hpx::cout << "=============================================================\n" << hpx::flush;
    
    // Summary from locality 0
    if (localId == 0) {
      hpx::cout << "\n*** If only locality 0 has stats above, work distribution FAILED ***\n" << hpx::flush;
    }
  }

  return hpx::finalize();
}

// main function to parse command line arguments and launch HPX runtime
int main(int argc, char *argv[]) {
  hpx::program_options::options_description desc("Vertex Cover — YewPar");

  desc.add_options()
    ("skeleton",
      hpx::program_options::value<std::string>()->default_value("seq"),
      "Skeleton: seq, depthbounded, stacksteal, budget, ordered")
    ("spawn-depth,d",
      hpx::program_options::value<std::uint64_t>()->default_value(0),
      "Spawn depth for parallel skeletons")
    ("backtrack-budget,b",
      hpx::program_options::value<unsigned>()->default_value(50),
      "Backtrack budget for budget skeleton")
    ("input-file,f",
      hpx::program_options::value<std::string>()->required(),
      "DIMACS graph")
    ("chunked", "Use chunking for stacksteal skeleton")
    ("discrepancyOrder", "Use discrepancy search in ordered skeleton");

  YewPar::registerPerformanceCounters();

  hpx::init_params args;
  args.desc_cmdline = desc;
  return hpx::init(argc, argv, args);
}