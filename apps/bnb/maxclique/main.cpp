#include <iostream>
#include <numeric>
#include <algorithm>
#include <vector>
#include <map>
#include <chrono>
#include <memory>
#include <typeinfo>

#include <hpx/hpx.hpp>
#include <hpx/hpx_init.hpp>

#include <boost/serialization/access.hpp>

#include "DimacsParser.hpp"
#include "BitGraph.hpp"
#include "BitSet.hpp"

#include "bnb/bnb-seq.hpp"
#include "bnb/bnb-par.hpp"
#include "bnb/bnb-dist.hpp"
#include "bnb/bnb-decision-seq.hpp"
#include "bnb/bnb-decision-par.hpp"
#include "bnb/bnb-decision-dist.hpp"
#include "bnb/ordered.hpp"
#include "bnb/macros.hpp"

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

using MCNode = hpx::util::tuple<MCSol, int, BitSet<NWORDS> >;

struct GenNode : skeletons::BnB::NodeGenerator<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS> > {
  std::array<unsigned, NWORDS * bits_per_word> p_order;
  std::array<unsigned, NWORDS * bits_per_word> colourClass;

  MCSol childSol;
  int childBnd;
  BitSet<NWORDS> p;

  int v;

  GenNode() {};
  GenNode(const MCNode & n,
          const std::array<unsigned, NWORDS * bits_per_word> p_order,
          const std::array<unsigned, NWORDS * bits_per_word> colourClass)
    : p_order(p_order), colourClass(colourClass) {

    childSol = hpx::util::get<0>(n);
    childBnd = hpx::util::get<1>(n) + 1;
    p = hpx::util::get<2>(n);
    numChildren = p.popcount();
    v = numChildren - 1;
  }

  // Get the next value
  MCNode next(const BitGraph<NWORDS> & graph, const MCNode & n) override {
    auto sol = childSol;
    sol.members.push_back(p_order[v]);
    sol.colours = colourClass[v] - 1;

    auto cands = p;
    graph.intersect_with_row(p_order[v], cands);

    // Side effectful function update
    p.unset(p_order[v]);
    v--;

    return hpx::util::make_tuple(std::move(sol), childBnd, std::move(cands));
  }
};

GenNode
generateChoices(const BitGraph<NWORDS> & graph, const MCNode & n) {
  std::array<unsigned, NWORDS * bits_per_word> p_order;
  std::array<unsigned, NWORDS * bits_per_word> colourClass;
  auto p = hpx::util::get<2>(n);

  colour_class_order(graph, p, p_order, colourClass);

  GenNode g(n, std::move(p_order), std::move(colourClass));
  return g;
}

int upperBound(const BitGraph<NWORDS> & space, const MCNode & n) {
  return hpx::util::get<1>(n) + hpx::util::get<0>(n).colours;
}

HPX_PLAIN_ACTION(generateChoices, generateChoices_act);
HPX_PLAIN_ACTION(upperBound, upperBound_act);
YEWPAR_CREATE_BNB_PAR_ACTION(par_act, BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>, generateChoices_act, upperBound_act, true);
YEWPAR_CREATE_BNB_DECISION_PAR_ACTION(decision_par_act, BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>, generateChoices_act, upperBound_act, true);
YEWPAR_CREATE_BNB_DIST_ACTION(dist_act, BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>, generateChoices_act, upperBound_act, true);
YEWPAR_CREATE_BNB_DECISION_DIST_ACTION(decision_dist_act, BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>, generateChoices_act, upperBound_act, true);

YEWPAR_CREATE_BNB_ORDERED_ACTION(ordered_act, BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>, generateChoices_act, upperBound_act, true);

typedef BitSet<NWORDS> bitsetType;
REGISTER_INCUMBENT(MCSol, int, bitsetType);
REGISTER_REGISTRY(BitGraph<NWORDS>, int);

int hpx_main(boost::program_options::variables_map & opts) {
  auto inputFile = opts["input-file"].as<std::string>();
  if (inputFile.empty()) {
    hpx::finalize();
    return EXIT_FAILURE;
  }

  const std::vector<std::string> skeletonTypes = {"seq", "par", "dist", "seq-decision", "par-decision", "dist-decision", "ordered"};

  auto skeletonType = opts["skeleton-type"].as<std::string>();
  auto found = std::find(std::begin(skeletonTypes), std::end(skeletonTypes), skeletonType);
  if (found == std::end(skeletonTypes)) {
    std::cout << "Invalid skeleton type option. Should be: seq, par or dist" << std::endl;
    hpx::finalize();
    return EXIT_FAILURE;
  }

  auto gFile = dimacs::read_dimacs(inputFile);

  // Order the graph (keep a hold of the map)
  std::map<int, int> invMap;
  auto graph = orderGraphFromFile<NWORDS>(gFile, invMap);

  auto spawnDepth = opts["spawn-depth"].as<std::uint64_t>();

  auto start_time = std::chrono::steady_clock::now();

  // Initialise Root Node
  MCSol mcsol;
  mcsol.members.reserve(graph.size());
  mcsol.colours = 0;

  BitSet<NWORDS> cands;
  cands.resize(graph.size());
  cands.set_all();
  auto root = hpx::util::make_tuple(mcsol, 0, cands);

  auto sol = root;
  if (skeletonType == "seq") {
    sol = skeletons::BnB::Seq::search<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>, decltype(generateChoices), decltype(upperBound), true>
      (graph, root, generateChoices, upperBound);
    std::cout << "Exapnds = " << skeletons::BnB::Seq::numExpands << std::endl;
  }
  if (skeletonType == "par") {
    sol = skeletons::BnB::Par::search<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>,
                                      generateChoices_act, upperBound_act, par_act, true>
      (spawnDepth, graph, root);
  }
  if (skeletonType == "dist") {
    sol = skeletons::BnB::Dist::search<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>,
                                       generateChoices_act, upperBound_act, dist_act, true>
      (spawnDepth, graph, root);
  }
  if (skeletonType == "seq-decision") {
    auto decisionBound = opts["decisionBound"].as<int>();
    sol = skeletons::BnB::Decision::Seq::search<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>, decltype(generateChoices), decltype(upperBound), true>
      (graph, root, decisionBound, generateChoices, upperBound);
    std::cout << "Expands = " << skeletons::BnB::Decision::Seq::numExpands << std::endl;
  }
  if (skeletonType == "par-decision") {
    auto decisionBound = opts["decisionBound"].as<int>();
    sol = skeletons::BnB::Decision::Par::search<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>,
                                      generateChoices_act, upperBound_act, decision_par_act, true>
      (spawnDepth, graph, root, decisionBound);
  }
  if (skeletonType == "dist-decision") {
    auto decisionBound = opts["decisionBound"].as<int>();
    sol = skeletons::BnB::Decision::Dist::search<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>,
                                                generateChoices_act, upperBound_act, decision_dist_act, true>
      (spawnDepth, graph, root, decisionBound);
  }
  if (skeletonType == "ordered") {
    sol = skeletons::BnB::Ordered::search<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>,
                                       generateChoices_act, upperBound_act, ordered_act, true>
      (spawnDepth, graph, root);
  }

  auto overall_time = std::chrono::duration_cast<std::chrono::milliseconds>
    (std::chrono::steady_clock::now() - start_time);

  auto maxCliqueSize = hpx::util::get<1>(sol);

  std::cout << "MaxClique Size = " << maxCliqueSize << std::endl;
  std::cout << "cpu = " << overall_time.count() << std::endl;

  return hpx::finalize(0); // End instantly. Required as the decision skeleton currently can't kill all threads.
}

int main (int argc, char* argv[]) {
  boost::program_options::options_description
    desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

  desc_commandline.add_options()
    ( "spawn-depth,d",
      boost::program_options::value<std::uint64_t>()->default_value(0),
      "Depth in the tree to spawn at"
      )
    ( "input-file,f",
      boost::program_options::value<std::string>(),
      "DIMACS formatted input graph"
      )
    ( "skeleton-type",
      boost::program_options::value<std::string>()->default_value("seq"),
      "Which skeleton to use"
      )
    ( "decisionBound",
    boost::program_options::value<int>()->default_value(0),
    "For Decision Skeletons. Size of the clique to search for"
    );

  hpx::register_startup_function(&workstealing::registerPerformanceCounters);

  return hpx::init(desc_commandline, argc, argv);
}


