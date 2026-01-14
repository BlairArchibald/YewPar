#include <iostream>
#include <numeric>
#include <algorithm>
#include <vector>
#include <chrono>
#include <limits>

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

#ifndef NWORDS
#define NWORDS 16
#endif

// Build BitGraph from DIMACS 
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

struct VCNode {
  friend class boost::serialization::access;

  int size = 0;
  BitSet<NWORDS> inCover;
  BitSet<NWORDS> active;
  bool isCover = false;

  int getObj() const {
    if (!isCover) return std::numeric_limits<int>::max();
    return size;
  }

  template <class Archive>
  void serialize(Archive &ar, const unsigned int) {
    ar & size;
    ar & inCover;
    ar & active;
    ar & isCover;
  }
};

// Helper: undecided vertices = active \ inCover
static inline BitSet<NWORDS> undecided_vertices(const VCNode &n) {
  BitSet<NWORDS> u = n.active;
  u.intersect_with_complement(n.inCover);
  return u;
}

// Check whether all edges induced by undecided vertices are covered by inCover
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

// Reduction rule: degree-0 in the undecided induced subgraph
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
// LB = |C| + size of a greedy maximal matching on the undecided induced subgraph
int vcBound(const BitGraph<NWORDS> &g, const VCNode &n) {
  int N = g.size();

  BitSet<NWORDS> avail = undecided_vertices(n);

  BitSet<NWORDS> used;
  used.resize(N);
  used.reset_all();

  int matchingSize = 0;

  for (int u = 0; u < N; ++u) {
    if (!avail.test(u) || used.test(u)) continue;

    BitSet<NWORDS> nbrs = avail;
    g.intersect_with_row(u, nbrs);        
    nbrs.intersect_with_complement(used); 

    int v = nbrs.first_set_bit();
    if (v != -1) {
      used.set(u);
      used.set(v);
      avail.unset(u);
      avail.unset(v);
      ++matchingSize;
    } 
    else {
      avail.unset(u);
    }
  }
  return n.size + matchingSize;
}

typedef func<decltype(&vcBound), &vcBound> vcBound_func;

// NodeGenerator: pick a high-degree vertex and branch on in cover / not in cover
struct VCGenNode : YewPar::NodeGenerator<VCNode, BitGraph<NWORDS>> {
  const BitGraph<NWORDS> &graph;
  VCNode parent;
  int next_child;
  int branchVertex;

  VCGenNode(const BitGraph<NWORDS> &g, const VCNode &node)
      : graph(g), parent(node), next_child(0), branchVertex(-1) {

    if (parent.isCover || parent.active.empty()) {
      numChildren = 0;
      return;
    }

    int N = graph.size();

    BitSet<NWORDS> undec = parent.active;
    undec.intersect_with_complement(parent.inCover);

    int bestDeg = -1;
    int bestV = -1;

    for (int u = 0; u < N; ++u) {
      if (!undec.test(u)) continue;

      BitSet<NWORDS> nbrs = undec;
      graph.intersect_with_row(u, nbrs);

      int d = static_cast<int>(nbrs.popcount());
      if (d > bestDeg) {
        bestDeg = d;
        bestV = u;
      }
    }

    if (bestV == -1) {
      parent.isCover = check_is_cover(graph, parent);
      numChildren = 0;
      return;
    }
    branchVertex = bestV;
    numChildren = 2;
  }

  VCNode include_vertex(int v) const {
    VCNode child = parent;
    if (!child.inCover.test(v)) {
      child.inCover.set(v);
      child.size += 1;
    }
    apply_reductions(graph, child);
    return child;
  }

  VCNode exclude_vertex(int v) const {
    VCNode child = parent;
    int N = graph.size();

    BitSet<NWORDS> nbrs = child.active;
    graph.intersect_with_row(v, nbrs); // neighbors among active vertices

    for (int u = 0; u < N; ++u) {
      if (!nbrs.test(u)) continue;
      if (!child.inCover.test(u)) {
        child.inCover.set(u);
        child.size += 1;
      }
    }

    child.active.unset(v);
    apply_reductions(graph, child);
    return child;
  }

  VCNode next() override {
    if (next_child >= numChildren) {
      return parent; // unused
    }

    VCNode out = (next_child == 0)
      ? include_vertex(branchVertex)
      : exclude_vertex(branchVertex);

    ++next_child;
    return out;
  }
};

// HPX main
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
  if (skeletonType == "seq") {
    sol = YewPar::Skeletons::Seq<VCGenNode,
          YewPar::Skeletons::API::Optimisation,
          YewPar::Skeletons::API::BoundFunction<vcBound_func>,
          YewPar::Skeletons::API::ObjectiveComparison<std::less<int>>>
          ::search(graph, root, P);
  }
  else if (skeletonType == "depthbounded") {
    P.spawnDepth = opts["spawn-depth"].as<std::uint64_t>();
    sol = YewPar::Skeletons::DepthBounded<VCGenNode,
          YewPar::Skeletons::API::Optimisation,
          YewPar::Skeletons::API::BoundFunction<vcBound_func>,
          YewPar::Skeletons::API::ObjectiveComparison<std::less<int>>>
          ::search(graph, root, P);
  }
  else if (skeletonType == "stacksteal") {
    P.stealAll = static_cast<bool>(opts.count("chunked"));
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
    } 
    else {
      sol = YewPar::Skeletons::Ordered<VCGenNode,
            YewPar::Skeletons::API::Optimisation,
            YewPar::Skeletons::API::BoundFunction<vcBound_func>,
            YewPar::Skeletons::API::ObjectiveComparison<std::less<int>>>
            ::search(graph, root, P);
    }
  }
  else if (skeletonType == "budget") {
    P.backtrackBudget = opts["backtrack-budget"].as<unsigned>();
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

  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);

  hpx::cout << "Minimum Vertex Cover Size = " << sol.size << "\n";
  hpx::cout << "cpu = " << ms.count() << " ms\n";

  return hpx::finalize();
}

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