#include <iostream>
#include <numeric>
#include <algorithm>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <typeinfo>

#include <hpx/hpx_init.hpp>

#include <boost/serialization/access.hpp>

#include "VFParser.hpp"
#include "BitGraph.hpp"
#include "BitSet.hpp"

//#include "skeletons/Seq.hpp"
#include "skeletons/DepthSpawning.hpp"
//#include "skeletons/StackStealing.hpp"

// Number of Words to use in our bitset representation
// Possible to specify at compile to to handler bigger graphs if required
#ifndef NWORDS
#define NWORDS 8
#endif

using Association = std::map<std::pair<unsigned, unsigned>, unsigned>;
using AssociatedEdges = std::vector<std::pair<unsigned, unsigned> >;

template<unsigned n_words_>
std::tuple<BitGraph<n_words_>, std::vector<int>, std::vector<int> >
buildGraph(const std::pair<Association, AssociatedEdges> & g) {
  std::vector<int> order(g.first.size());
  std::vector<int> invorder(g.first.size());
  std::iota(order.begin(), order.end(), 0);

  // Pre-calculate degrees
  std::vector<int> degrees;
  degrees.resize(g.first.size());
  for (auto & f : g.second) {
    ++degrees[f.first];
    ++degrees[f.second];
  }

  std::sort(order.begin(), order.end(),
            [&] (int a, int b) { return true ^ (degrees[a] < degrees[b] || (degrees[a] == degrees[b] && a > b)); });

  // Encode BitGraph
  BitGraph<n_words_> graph;
  graph.resize(g.first.size());

  for (unsigned i = 0 ; i < order.size() ; ++i)
    invorder[order[i]] = i;

  for (auto & f : g.second)
    graph.add_edge(invorder[f.first], invorder[f.second]);

  return std::make_tuple(graph, order, invorder);
}


auto modular_product(const VFGraph & g1, const VFGraph & g2) -> std::pair<Association, AssociatedEdges>
{
  unsigned next_vertex = 0;
  Association association;
  AssociatedEdges edges;

  for (unsigned l = 0 ; l < std::min(g1.vertices_by_label.size(), g2.vertices_by_label.size()) ; ++l)
      for (unsigned v1 : g1.vertices_by_label[l])
          for (unsigned v2 : g2.vertices_by_label[l])
              for (unsigned m = 0 ; m < std::min(g1.vertices_by_label.size(), g2.vertices_by_label.size()) ; ++m)
                  for (unsigned w1 : g1.vertices_by_label[m])
                      for (unsigned w2 : g2.vertices_by_label[m])
                          if (v1 != w1 && v2 != w2
                                  && (g1.edges[v1][v1] == g2.edges[v2][v2])
                                  && (g1.edges[w1][w1] == g2.edges[w2][w2])
                                  && (g1.edges[v1][w1] == g2.edges[v2][w2])
                                  && (g1.edges[w1][v1] == g2.edges[w2][v2])) {

                              if (! association.count({v1, v2}))
                                  association.insert({{v1, v2}, next_vertex++});
                              if (! association.count({w1, w2}))
                                  association.insert({{w1, w2}, next_vertex++});

                              edges.push_back({association.find({v1, v2})->second, association.find({w1, w2})->second});
                          }

  return { association, edges };
}

auto unproduct(const Association & association, unsigned v) -> std::pair<unsigned, unsigned>
{
  for (auto & a : association)
    if (a.second == v)
      return a.first;

  throw "oops";
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
};


int upperBound(const BitGraph<NWORDS> & space, const MCNode & n) {
  return n.size + n.sol.colours;
}

typedef func<decltype(&upperBound), &upperBound> upperBound_func;


REGISTER_INCUMBENT(MCNode);

// using ss_skel = YewPar::Skeletons::StackStealing<GenNode,
//                                                  YewPar::Skeletons::API::BnB,
//                                                  YewPar::Skeletons::API::BoundFunction<upperBound_func>,
//                                                  YewPar::Skeletons::API::PruneLevel>;
// using cfunc  = ss_skel::SubTreeTask;
// REGISTER_SEARCHMANAGER(MCNode, cfunc);

int hpx_main(boost::program_options::variables_map & opts) {
  auto patternF = opts["pattern-file"].as<std::string>();
  auto targetF  = opts["target-file"].as<std::string>();

  auto patternG = read_vf(patternF, opts.count("unlabelled"), opts.count("no-edge-labels"), opts.count("undirected"));
  auto targetG = read_vf(targetF, opts.count("unlabelled"), opts.count("no-edge-labels"), opts.count("undirected"));

  auto prod = modular_product(patternG, targetG);

  BitGraph<NWORDS> graph; std::vector<int> order, invorder;
  std::tie(graph, order, invorder) = buildGraph<NWORDS>(prod);

  if (graph.size() > bits_per_word*NWORDS) {
    std::cout << "Binary Cannot Handle Graph of this size. Recompile with a bigger NWORDS" << std::endl;
    hpx::finalize();
    return EXIT_FAILURE;
  }

  auto spawnDepth = opts["spawn-depth"].as<std::uint64_t>();
  auto start_time = std::chrono::steady_clock::now();

  // // Initialise Root Node
  MCSol mcsol;
  mcsol.members.reserve(graph.size());
  mcsol.colours = 0;

  BitSet<NWORDS> cands;
  cands.resize(graph.size());
  cands.set_all();
  MCNode root = {mcsol, 0, cands};

  YewPar::Skeletons::API::Params<int> searchParameters;
  searchParameters.spawnDepth = spawnDepth;
  auto result = YewPar::Skeletons::DepthSpawns<GenNode,
                                               YewPar::Skeletons::API::BnB,
                                               YewPar::Skeletons::API::BoundFunction<upperBound_func>,
                                               YewPar::Skeletons::API::PruneLevel>
        ::search(graph, root, searchParameters);

  auto overall_time = std::chrono::duration_cast<std::chrono::milliseconds>
    (std::chrono::steady_clock::now() - start_time);

  std::map<int, int> isomorphism;
  auto sol = result.sol;
  for (auto const & v : sol.members) {
    isomorphism.insert(unproduct(prod.first, order[v]));
  }

  // Print Results
  std::cout << std::boolalpha << ! isomorphism.empty() << " " << 0;
  std::cout << " " << isomorphism.size() << std::endl;

  for (auto v : isomorphism)
      std::cout << "(" << v.first << " -> " << v.second << ") ";
  std::cout << std::endl;

  std::cout << overall_time.count() << std::endl;

  auto graphs = std::make_pair(patternG, targetG);
  for (auto & p : isomorphism) {
      if (graphs.first.vertex_labels.at(p.first) != graphs.second.vertex_labels.at(p.second)) {
          std::cerr << "Oops! not an isomorphism due to vertex labels" << std::endl;
          hpx::finalize();
          return EXIT_FAILURE;
      }
  }

  for (auto & p : isomorphism) {
      for (auto & q : isomorphism) {
          if (graphs.first.edges.at(p.first).at(q.first) != graphs.second.edges.at(p.second).at(q.second)) {
              std::cerr << "Oops! not an isomorphism due to edge " << std::to_string(p.first) << "--"
                  << std::to_string(q.first) << " (" << graphs.first.edges[p.first][q.first] << ") vs "
                  << std::to_string(p.second) << "--" << std::to_string(q.second) << " ("
                  << graphs.second.edges[p.second][q.second] << ")" << std::endl;
              hpx::finalize();
              return EXIT_FAILURE;
          }
      }
  }

  return hpx::finalize();
}

int main (int argc, char* argv[]) {
  boost::program_options::options_description
    desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

  desc_commandline.add_options()
    ( "spawn-depth,d",
      boost::program_options::value<std::uint64_t>()->default_value(0),
      "Depth in the tree to spawn at"
      )
    ( "pattern-file",
      boost::program_options::value<std::string>(),
      "VF formatted input graph"
      )
    ( "target-file",
      boost::program_options::value<std::string>(),
      "VF formatted input graph"
    )
    ("unlabelled", "Make the graph unlabelled")
    ("no-edge-labels", "Get rid of edge labels, but keep vertex labels")
    ("undirected", "Make the graph undirected");

  return hpx::init(desc_commandline, argc, argv);
}


