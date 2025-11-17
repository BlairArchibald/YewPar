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

    // Build complement graph manually
    BitGraph<n_words_> comp;
    comp.resize(N);

    for (int u = 0; u < N; ++u) {
        for (int v = u + 1; v < N; ++v) {

            // If (u,v) is *not* an edge in the original graph, add it to the complement
            if (!original.adjacent(u, v)) {
                comp.add_edge(u, v);
            }

            // If it's an edge, don't add anything (complement omits it)
        }
    }
    return comp;
}

// Vertex Cover node and solution state
struct VCSol {
  std::vector<int> cover;

  template <class Archive>
  void serialize(Archive &ar, const unsigned int) {
    ar & cover;
  }
};

struct VCNode {
  friend class boost::serialization::access;

  VCSol sol;
  int size;                  
  BitSet<NWORDS> inCover;    
  BitSet<NWORDS> active;     
  bool isCover;              

  int getObj() const {
    if (!isCover)
      return std::numeric_limits<int>::max();
    return size;
  }

  template <class Archive>
  void serialize(Archive &ar, const unsigned int) {
    ar & sol;
    ar & size;
    ar & inCover;
    ar & active;
    ar & isCover;
  }
};

// Helpers: degrees, coverage check, etc, over BitGraph + node state

// Compute degree of a vertex u in the residual graph (active vertices only)
static int residual_degree(const BitGraph<NWORDS> &g, const VCNode &n, int u) {
  if (!n.active.test(u)) return 0;
  BitSet<NWORDS> nbrs = n.active;
  g.intersect_with_row(u, nbrs);
  return (int)nbrs.popcount();
}

// Check whether all edges induced by active are covered by inCover
static bool check_is_cover(const BitGraph<NWORDS> &g, const VCNode &n) {
  int N = g.size();

  // vertices not in cover but still active
  BitSet<NWORDS> notInCover = n.active;
  for (int i = 0; i < N; ++i)
    if (n.inCover.test(i))
      notInCover.unset(i);

  for (int u = 0; u < N; ++u) {
    if (!notInCover.test(u)) continue;

    BitSet<NWORDS> nbrs = notInCover;
    g.intersect_with_row(u, nbrs);
    if (!nbrs.empty()) {
      // found an edge (u,v) with neither in cover
      return false;
    }
  }
  return true;
}

// Reduction rules (R1 + R2) applied to a VCNode
static bool apply_reductions(const BitGraph<NWORDS> &g, VCNode &n) {
  bool changed = false;
  int N = g.size();

  while (true) {
    bool localChange = false;

    // recompute "not in cover & active"
    BitSet<NWORDS> undecided = n.active;
    for (int i = 0; i < N; ++i)
      if (n.inCover.test(i))
        undecided.unset(i);

    // Degree-0 rule: remove isolated vertices (in residual)
    for (int u = 0; u < N; ++u) {
      if (!undecided.test(u)) continue;

      BitSet<NWORDS> nbrs = n.active;
      g.intersect_with_row(u, nbrs);
      // remove neighbours already in cover (edges to them are already covered)
      for (int v = 0; v < N; ++v)
        if (nbrs.test(v) && n.inCover.test(v))
          nbrs.unset(v);

      if (nbrs.empty()) {
        // u has no uncovered edges, then can be removed from active
        n.active.unset(u);
        undecided.unset(u);
        localChange = true;
      }
    }
    changed = changed || localChange;
    if (!localChange) break;
  }
  n.isCover = check_is_cover(g, n);
  return changed;
}

// Matching-based lower bound on remaining cover size
// LB = |C| + size of a greedy maximal matching on uncovered residual edges
int vcBound(const BitGraph<NWORDS> &g, const VCNode &n) {
  int N = g.size();

  // Build set of vertices that can participate in uncovered edges:
  BitSet<NWORDS> avail = n.active;
  for (int i = 0; i < N; ++i)
    if (n.inCover.test(i))
      avail.unset(i);

  std::vector<bool> used(N, false);
  int matchingSize = 0;

  for (int u = 0; u < N; ++u) {
    if (!avail.test(u) || used[u]) continue;

    BitSet<NWORDS> nbrs = avail;
    g.intersect_with_row(u, nbrs);

    // filter out used vertices
    for (int v = 0; v < N; ++v)
      if (nbrs.test(v) && used[v])
        nbrs.unset(v);

    int v = nbrs.first_set_bit();
    if (v != -1) {
      // match (u,v)
      used[u] = used[v] = true;
      avail.unset(u);
      avail.unset(v);
      matchingSize++;
    } else {
      avail.unset(u);
    }
  }
  return n.size + matchingSize;
}

typedef func<decltype(&vcBound), &vcBound> vcBound_func;

// NodeGenerator: pick a high-degree vertex and branch on “in cover / not in cover”
struct VCGenNode : YewPar::NodeGenerator<VCNode, BitGraph<NWORDS>> {

  const BitGraph<NWORDS> &graph;
  VCNode parent;
  int next_child;
  int branchVertex;  // vertex v we branch on

  VCGenNode(const BitGraph<NWORDS> &g, const VCNode &node)
      : graph(g), parent(node), next_child(0), branchVertex(-1) {

    // If parent is already a full cover or there are no active vertices, stop
    if (parent.isCover || parent.active.empty()) {
      numChildren = 0;
      return;
    }

    int N = graph.size();

    // Choose branching vertex: active, not in cover, with maximum residual degree
    int bestDeg = -1;
    int bestV = -1;

    for (int u = 0; u < N; ++u) {
      if (!parent.active.test(u)) continue;
      if (parent.inCover.test(u)) continue;

      BitSet<NWORDS> nbrs = parent.active;
      graph.intersect_with_row(u, nbrs);
      // remove neighbors already in cover (edges already covered)
      for (int v = 0; v < N; ++v)
        if (nbrs.test(v) && parent.inCover.test(v))
          nbrs.unset(v);

      int d = (int)nbrs.popcount();
      if (d > bestDeg) {
        bestDeg = d;
        bestV = u;
      }
    }

    if (bestV == -1) {
      // No vertex to branch on. Either it's a cover or something degenerated
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
      child.sol.cover.push_back(v);
      child.size += 1;
    }
    // v stays active; edges incident to v are covered, but other vertices still matter
    apply_reductions(graph, child);
    return child;
  }

  VCNode exclude_vertex(int v) const {
    VCNode child = parent;
    int N = graph.size();

    // If we exclude v from the cover, then for every neighbor u of v
    // that is still active and not already in cover, we must include u
    BitSet<NWORDS> nbrs = child.active;
    graph.intersect_with_row(v, nbrs);

    for (int u = 0; u < N; ++u) {
      if (!nbrs.test(u)) continue;
      if (!child.inCover.test(u)) {
        child.inCover.set(u);
        child.sol.cover.push_back(u);
        child.size += 1;
      }
    }

    // v itself can be removed from active; it will never enter the cover
    child.active.unset(v);

    apply_reductions(graph, child);
    return child;
  }

  VCNode next() override {
    if (next_child >= numChildren)
      return parent; // won't be used

    VCNode out;
    if (next_child == 0) {
      // Branch 1: include branchVertex
      out = include_vertex(branchVertex);
    } else {
      // Branch 2: exclude branchVertex (so all its neighbors go to cover)
      out = exclude_vertex(branchVertex);
    }

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

  // Root node: no vertices chosen, all active
  VCNode root;
  root.size = 0;
  root.sol.cover.clear();
  root.inCover.resize(graph.size());
  root.inCover.reset_all();
  root.active.resize(graph.size());
  root.active.set_all();
  root.isCover = check_is_cover(graph, root);

  // Apply reductions once at the root
  apply_reductions(graph, root);

  VCNode sol = root;

  auto skeletonType = opts["skeleton"].as<std::string>();
  auto spawnDepth   = opts["spawn-depth"].as<std::uint64_t>();

  using OptTag = YewPar::Skeletons::API::Optimisation;
  using BoundT = YewPar::Skeletons::API::BoundFunction<vcBound_func>;
  using MinCmp = YewPar::Skeletons::API::ObjectiveComparison<std::less<int>>;

  YewPar::Skeletons::API::Params<int> P;
  P.initialBound = graph.size(); // worst-case cover size ≤ |V|

  if (skeletonType == "seq") {
    sol = YewPar::Skeletons::Seq<VCGenNode,
                                 OptTag,
                                 BoundT,
                                 MinCmp>
            ::search(graph, root, P);

  } else if (skeletonType == "depthbounded") {
    P.spawnDepth = spawnDepth;
    sol = YewPar::Skeletons::DepthBounded<VCGenNode,
                                          OptTag,
                                          BoundT,
                                          MinCmp>
            ::search(graph, root, P);

  } else if (skeletonType == "stacksteal") {
    P.stealAll = static_cast<bool>(opts.count("chunked"));
    sol = YewPar::Skeletons::StackStealing<VCGenNode,
                                           OptTag,
                                           BoundT,
                                           MinCmp>
            ::search(graph, root, P);

  } else if (skeletonType == "ordered") {
    P.spawnDepth = spawnDepth;
    if (opts.count("discrepancyOrder")) {
      sol = YewPar::Skeletons::Ordered<VCGenNode,
                                       OptTag,
                                       BoundT,
                                       MinCmp,
                                       YewPar::Skeletons::API::DiscrepancySearch>
              ::search(graph, root, P);
    } else {
      sol = YewPar::Skeletons::Ordered<VCGenNode,
                                       OptTag,
                                       BoundT,
                                       MinCmp>
              ::search(graph, root, P);
    }

  } else if (skeletonType == "budget") {
    P.backtrackBudget = opts["backtrack-budget"].as<unsigned>();
    sol = YewPar::Skeletons::Budget<VCGenNode,
                                    OptTag,
                                    BoundT,
                                    MinCmp>
            ::search(graph, root, P);

  } else {
    hpx::cout << "Invalid skeleton type\n";
    return hpx::finalize();
  }

  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - start_time);

  hpx::cout << "Minimum Vertex Cover Size = " << sol.size << "\n";
  hpx::cout << "cpu = " << ms.count() << " ms\n";

  return hpx::finalize();
}

// CLI 
int main(int argc, char *argv[]) {
  hpx::program_options::options_description
      desc("Vertex Cover — YewPar");

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