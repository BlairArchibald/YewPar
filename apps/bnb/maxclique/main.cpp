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
#include "bnb/bnb-recompute.hpp"
#include "bnb/bnb-indexed.hpp"
#include "bnb/ordered.hpp"

#include "util/func.hpp"

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

  const BitGraph<NWORDS> & graph;

  MCSol childSol;
  int childBnd;
  BitSet<NWORDS> p;

  int v;

  GenNode(const BitGraph<NWORDS> & graph,
          const MCNode & n,
          const std::array<unsigned, NWORDS * bits_per_word> p_order,
          const std::array<unsigned, NWORDS * bits_per_word> colourClass)
    : graph(graph), p_order(p_order), colourClass(colourClass) {

    childSol = hpx::util::get<0>(n);
    childBnd = hpx::util::get<1>(n) + 1;
    p = hpx::util::get<2>(n);
    numChildren = p.popcount();
    v = numChildren - 1;
  }

  // Get the next value
  MCNode next() override {
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

  MCNode nth(unsigned n) override {
    auto pos = v - n;


    auto sol = childSol;
    sol.members.push_back(p_order[pos]);
    sol.colours = colourClass[pos] - 1;

    auto cands = p;
    // Remove all choices from the left "left" of the one we care about
    for (auto i = v; i > pos; --i) {
      cands.unset(p_order[i]);
    }

    graph.intersect_with_row(p_order[pos], cands);

    return hpx::util::make_tuple(std::move(sol), childBnd, std::move(cands));
  }
};

GenNode
generateChoices(const BitGraph<NWORDS> & graph, const MCNode & n) {
  std::array<unsigned, NWORDS * bits_per_word> p_order;
  std::array<unsigned, NWORDS * bits_per_word> colourClass;
  auto p = hpx::util::get<2>(n);

  colour_class_order(graph, p, p_order, colourClass);

  GenNode g(graph, n, std::move(p_order), std::move(colourClass));
  return g;
}

int upperBound(const BitGraph<NWORDS> & space, const MCNode & n) {
  return hpx::util::get<1>(n) + hpx::util::get<0>(n).colours;
}

typedef func<decltype(&generateChoices), &generateChoices> generateChoices_func;
typedef func<decltype(&upperBound), &upperBound> upperBound_func;

// We want large stacks for everything
using par_act = skeletons::BnB::Par::BranchAndBoundOpt<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>, generateChoices_func, upperBound_func, true>::ChildTask;
HPX_ACTION_USES_LARGE_STACK(par_act);
using decision_par_act = skeletons::BnB::Par::BranchAndBoundSat<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>, generateChoices_func, upperBound_func, true>::ChildTask;
HPX_ACTION_USES_LARGE_STACK(decision_par_act);
using dist_act = skeletons::BnB::Dist::BranchAndBoundOpt<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>, generateChoices_func, upperBound_func, true>::ChildTask;
HPX_ACTION_USES_LARGE_STACK(dist_act);
using decision_dist_act = skeletons::BnB::Dist::BranchAndBoundSat<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>, generateChoices_func, upperBound_func, true>::ChildTask;
HPX_ACTION_USES_LARGE_STACK(decision_dist_act);
using ordered_act = skeletons::BnB::Ordered::BranchAndBoundOpt<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>, generateChoices_func, upperBound_func, true>::ChildTask;
HPX_ACTION_USES_LARGE_STACK(ordered_act);
using recompute_act = skeletons::BnB::DistRecompute::BranchAndBoundOpt<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>, generateChoices_func, upperBound_func, true>::ChildTask;
HPX_ACTION_USES_LARGE_STACK(recompute_act);

typedef BitSet<NWORDS> bitsetType;
REGISTER_INCUMBENT(MCSol, int, bitsetType);
REGISTER_REGISTRY(BitGraph<NWORDS>, MCSol, int, bitsetType);

using indexedFunc = skeletons::BnB::Indexed::BranchAndBoundOpt<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>, generateChoices_func, upperBound_func, true>::ChildTask;
using pathType = std::vector<unsigned>;
REGISTER_SEARCHMANAGER(pathType, indexedFunc)

int hpx_main(boost::program_options::variables_map & opts) {
  auto inputFile = opts["input-file"].as<std::string>();
  if (inputFile.empty()) {
    hpx::finalize();
    return EXIT_FAILURE;
  }

  const std::vector<std::string> skeletonTypes = {"seq", "par", "dist", "seq-decision", "par-decision", "dist-decision", "ordered", "dist-recompute", "indexed"};

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
    sol = skeletons::BnB::Seq::BranchAndBoundOpt<BitGraph<NWORDS>,
                                                 MCSol,
                                                 int,
                                                 BitSet<NWORDS>,
                                                 generateChoices_func,
                                                 upperBound_func,
                                                 true>
          ::search (graph, root);
  }
  if (skeletonType == "par") {
    sol = skeletons::BnB::Par::BranchAndBoundOpt<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>, generateChoices_func, upperBound_func, true>
          ::search(spawnDepth, graph, root);
  }
  if (skeletonType == "dist") {
    sol = skeletons::BnB::Dist::BranchAndBoundOpt<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>,
                                       generateChoices_func, upperBound_func, true>
          ::search(spawnDepth, graph, root);
  }
  if (skeletonType == "seq-decision") {
    auto decisionBound = opts["decisionBound"].as<int>();
    sol = skeletons::BnB::Seq::BranchAndBoundSat<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>, generateChoices_func, upperBound_func, true>
          ::search(graph, root, decisionBound);
  }
  if (skeletonType == "par-decision") {
    auto decisionBound = opts["decisionBound"].as<int>();
    sol = skeletons::BnB::Par::BranchAndBoundSat<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>,
                                                 generateChoices_func, upperBound_func, true>
          ::search(spawnDepth, graph, root, decisionBound);
  }
  if (skeletonType == "dist-decision") {
    auto decisionBound = opts["decisionBound"].as<int>();
    sol = skeletons::BnB::Dist::BranchAndBoundSat<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>,
                                                  generateChoices_func, upperBound_func, true>
          ::search(spawnDepth, graph, root, decisionBound);
  }
  if (skeletonType == "ordered") {
    sol = skeletons::BnB::Ordered::BranchAndBoundOpt<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>,
                                                     generateChoices_func, upperBound_func, true>
          ::search(spawnDepth, graph, root);
  }
  if (skeletonType == "dist-recompute") {
    sol = skeletons::BnB::DistRecompute::BranchAndBoundOpt<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>,
                                                           generateChoices_func, upperBound_func, true>
          ::search(spawnDepth, graph, root);
  }
  if (skeletonType == "indexed") {
    sol = skeletons::BnB::Indexed::BranchAndBoundOpt<BitGraph<NWORDS>, MCSol, int, BitSet<NWORDS>,
                                                     generateChoices_func, upperBound_func, true>
          ::search(graph, root);
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
  hpx::register_startup_function(&workstealing::priority::registerPerformanceCounters);

  return hpx::init(desc_commandline, argc, argv);
}


