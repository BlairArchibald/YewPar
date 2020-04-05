#include "fixed_bit_set.hh"

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <numeric>
#include <random>
#include <tuple>
#include <utility>
#include <chrono>

#include "YewPar.hpp"

#include "skeletons/Seq.hpp"
#include "skeletons/DepthBounded.hpp"
#include "skeletons/StackStealing.hpp"
#include "skeletons/Ordered.hpp"
#include "skeletons/Budget.hpp"

#include "util/func.hpp"
#include "util/NodeGenerator.hpp"

#include "lad.hh"
#include "fixed_bit_set.hh"

#include <hpx/hpx_init.hpp>
#include <hpx/util/tuple.hpp>
#include <hpx/include/iostreams.hpp>

#ifndef NWORDS
#define NWORDS 64
#endif

using std::array;
using std::iota;
using std::fill;
using std::find_if;
using std::get;
using std::greater;
using std::list;
using std::max;
using std::map;
using std::move;
using std::mt19937;
using std::next;
using std::numeric_limits;
using std::pair;
using std::sort;
using std::string;
using std::swap;
using std::to_string;
using std::uniform_int_distribution;
using std::uniform_real_distribution;
using std::vector;

using hpx::util::tuple;

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

auto degree_sort(const Graph & graph, vector<int> & p, bool reverse) -> void
{
// pre-calculate degrees
vector<int> degrees;

for (int v = 0 ; v < graph.size() ; ++v)
  degrees.push_back(graph.degree(v));

// sort on degree
sort(p.begin(), p.end(),
       [&] (int a, int b) { return (! reverse) ^ (degrees[a] < degrees[b] || (degrees[a] == degrees[b] && a > b)); });
}

struct Assignment {
  unsigned variable;
  unsigned value;

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & variable;
    ar & value;
  }
};

struct Assignments {
    vector<tuple<Assignment, bool> > values;

    bool contains(const Assignment & assignment) const
    {
        // this should not be a linear scan...
        return values.end() != find_if(values.begin(), values.end(), [&] (const auto & a) {
                return get<0>(a).variable == assignment.variable && get<0>(a).value == assignment.value;
                });
    }

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & values;
  }
};

template <unsigned n_words_>
struct Domain
{
  unsigned v;
  unsigned popcount;
  bool fixed = false;
  FixedBitSet<n_words_> values;

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & v;
    ar & popcount;
    ar & fixed;
    ar & values;
  }
};

template <unsigned n_words_>
using Domains = vector<Domain<n_words_> >;

template <unsigned n_words_>
class Model {
private:
vector<uint8_t> _pattern_adjacencies_bits;

public:
int max_graphs;

unsigned pattern_size, full_pattern_size, target_size;

vector<FixedBitSet<n_words_> > pattern_graph_rows;
vector<FixedBitSet<n_words_> > target_graph_rows;

vector<int> pattern_permutation, target_permutation, isolated_vertices;
vector<vector<int> > patterns_degrees, targets_degrees;
int largest_target_degree;

  Model() = default;

  Model(const Graph & target, const Graph & pattern) :
      max_graphs(5),
      pattern_size(pattern.size()),
      full_pattern_size(pattern.size()),
      target_size(target.size()),
      target_permutation(target.size()),
      patterns_degrees(max_graphs),
      targets_degrees(max_graphs),
      largest_target_degree(0)
  {
    // strip out isolated vertices in the pattern, and build pattern_permutation
    for (unsigned v = 0 ; v < full_pattern_size ; ++v)
      if (0 == pattern.degree(v)) {
        isolated_vertices.push_back(v);
        --pattern_size;
      }
      else
        pattern_permutation.push_back(v);

    // recode pattern to a bit graph
    pattern_graph_rows.resize(pattern_size * max_graphs);
    for (unsigned i = 0 ; i < pattern_size ; ++i)
      for (unsigned j = 0 ; j < pattern_size ; ++j)
        if (pattern.adjacent(pattern_permutation.at(i), pattern_permutation.at(j)))
          pattern_graph_rows[i * max_graphs + 0].set(j);

    // determine ordering for target graph vertices
    iota(target_permutation.begin(), target_permutation.end(), 0);

    degree_sort(target, target_permutation, false);

    // recode target to a bit graph
    target_graph_rows.resize(target_size * max_graphs);
    for (unsigned i = 0 ; i < target_size ; ++i)
      for (unsigned j = 0 ; j < target_size ; ++j)
        if (target.adjacent(target_permutation.at(i), target_permutation.at(j)))
          target_graph_rows[i * max_graphs + 0].set(j);

    // build up supplemental graphs
    build_supplemental_graphs(pattern_graph_rows, pattern_size);
    build_supplemental_graphs(target_graph_rows, target_size);

    // pattern and target degrees, including supplemental graphs
    for (int g = 0 ; g < max_graphs ; ++g) {
      patterns_degrees.at(g).resize(pattern_size);
      targets_degrees.at(g).resize(target_size);
    }

    for (int g = 0 ; g < max_graphs ; ++g) {
      for (unsigned i = 0 ; i < pattern_size ; ++i)
        patterns_degrees.at(g).at(i) = pattern_graph_rows[i * max_graphs + g].popcount();

      for (unsigned i = 0 ; i < target_size ; ++i)
        targets_degrees.at(g).at(i) = target_graph_rows[i * max_graphs + g].popcount();
    }

    for (unsigned i = 0 ; i < target_size ; ++i)
      largest_target_degree = max(largest_target_degree, targets_degrees[0][i]);

    // pattern adjacencies, compressed
    _pattern_adjacencies_bits.resize(pattern_size * pattern_size);
    for (int g = 0 ; g < max_graphs ; ++g)
      for (unsigned i = 0 ; i < pattern_size ; ++i)
        for (unsigned j = 0 ; j < pattern_size ; ++j)
          if (pattern_graph_rows[i * max_graphs + g].test(j))
            pattern_adjacencies_bits(i, j) |= (1u << g);
  }

  auto pattern_adjacencies_bits(unsigned i, unsigned j) -> uint8_t &
  {
    return _pattern_adjacencies_bits[i * pattern_size + j];
  }

  auto pattern_adjacencies_bits(unsigned i, unsigned j) const -> const uint8_t &
  {
    return _pattern_adjacencies_bits[i * pattern_size + j];
  }

  auto build_supplemental_graphs(vector<FixedBitSet<n_words_> > & graph_rows, unsigned size) -> void
  {
    vector<vector<unsigned> > path_counts(size, vector<unsigned>(size, 0));

    // count number of paths from w to v (only w >= v, so not v to w)
    for (unsigned v = 0 ; v < size ; ++v) {
      auto nv = graph_rows[v * max_graphs + 0];
      unsigned cp = 0;
      for (int c = nv.first_set_bit_from(cp) ; c != -1 ; c = nv.first_set_bit_from(cp)) {
        nv.unset(c);
        auto nc = graph_rows[c * max_graphs + 0];
        unsigned wp = 0;
        for (int w = nc.first_set_bit_from(wp) ; w != -1 && w <= int(v) ; w = nc.first_set_bit_from(wp)) {
          nc.unset(w);
          ++path_counts[v][w];
        }
      }
    }

    for (unsigned v = 0 ; v < size ; ++v) {
      for (unsigned w = v ; w < size ; ++w) {
        // w to v, not v to w, see above
        unsigned path_count = path_counts[w][v];
        for (unsigned p = 1 ; p <= 4 ; ++p) {
          if (path_count >= p) {
            graph_rows[v * max_graphs + p].set(w);
            graph_rows[w * max_graphs + p].set(v);
          }
        }
      }
    }
  }

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & _pattern_adjacencies_bits;
    ar & max_graphs;
    ar & pattern_size;
    ar & full_pattern_size;
    ar & target_size;
    ar & pattern_graph_rows;
    ar & target_graph_rows;
    ar & pattern_permutation;
    ar & target_permutation;
    ar & isolated_vertices;
    ar & patterns_degrees;
    ar & targets_degrees;
    ar & largest_target_degree;
  }
};

template <unsigned n_words_>
auto find_unit_domain(Domains<n_words_> & domains) -> typename Domains<n_words_>::iterator
{
  return find_if(domains.begin(), domains.end(), [] (Domain<n_words_> & d) {
      return (! d.fixed) && 1 == d.popcount;
    });
}

// The max_graphs_ template parameter is so that the for each graph
// pair loop gets unrolled, which makes an annoyingly large difference
// to performance. Note that for larger target graphs, half of the
// total runtime is spent in this function.
template <unsigned n_words_, int max_graphs_>
auto propagate_adjacency_constraints(
    const Model<n_words_> & m,
    Domain<n_words_> & d,
    const Assignment & current_assignment) -> void
{
  auto pattern_adjacency_bits = m.pattern_adjacencies_bits(current_assignment.variable, d.v);

  // for each graph pair...
  for (int g = 0 ; g < max_graphs_ ; ++g) {
    // if we're adjacent...
    if (pattern_adjacency_bits & (1u << g)) {
      // ...then we can only be mapped to adjacent vertices
      d.values.intersect_with(m.target_graph_rows[current_assignment.value * max_graphs_ + g]);
    }
  }
}

template <unsigned n_words_>
auto propagate_simple_constraints(
    const Model<n_words_> & m,
    Domains<n_words_> & new_domains,
    const Assignment & current_assignment) -> bool
{
  // propagate for each remaining domain...
  for (auto & d : new_domains) {
    if (d.fixed)
      continue;

    // all different
    d.values.unset(current_assignment.value);

    // adjacency
    switch (m.max_graphs) {
      case 5: propagate_adjacency_constraints<n_words_, 5>(m, d, current_assignment); break;
      case 6: propagate_adjacency_constraints<n_words_, 6>(m, d, current_assignment); break;

      default:
        throw "you forgot to update the ugly max_graphs hack";
    }

    // we might have removed values
    d.popcount = d.values.popcount();
    if (0 == d.popcount)
      return false;
  }

  return true;
}

template <unsigned n_words_>
auto propagate(
    const Model<n_words_> & m,
    Domains<n_words_> & new_domains,
    Assignments & assignments) -> bool
{
  // whilst we've got a unit domain...
  for (typename Domains<n_words_>::iterator branch_domain = find_unit_domain(new_domains) ;
       branch_domain != new_domains.end() ;
       branch_domain = find_unit_domain(new_domains)) {

    // what are we assigning?
    Assignment current_assignment = { branch_domain->v, branch_domain->values.first_set_bit() };

    // ok, make the assignment
    branch_domain->fixed = true;
    assignments.values.push_back(hpx::util::make_tuple(current_assignment, false));
    //assignments.values.push_back({ current_assignment.variable, current_assignment.value}, false });

    // propagate simple all different and adjacency
    if (! propagate_simple_constraints(m, new_domains, current_assignment))
      return false;

    // propagate all different
    if (! cheap_all_different(new_domains))
      return false;
  }

  return true;
}

template <unsigned n_words_>
auto find_branch_domain(
    const Model<n_words_> & m,
    const Domains<n_words_> & domains) -> const Domain<n_words_> *
{
  const Domain<n_words_> * result = nullptr;
  for (auto & d : domains)
    if (! d.fixed)
      if ((! result) ||
          (d.popcount < result->popcount) ||
          (d.popcount == result->popcount && m.patterns_degrees[0][d.v] > m.patterns_degrees[0][result->v]))
        result = &d;
  return result;
}

template <unsigned n_words_>
auto copy_domains_and_assign(
    const Domains<n_words_> & domains,
    unsigned branch_v,
    unsigned f_v) -> Domains<n_words_>
{
  Domains<n_words_> new_domains;
  new_domains.reserve(domains.size());
  for (auto & d : domains) {
    if (d.fixed)
      continue;

    new_domains.push_back(d);
    if (d.v == branch_v) {
      new_domains.back().values.unset_all();
      new_domains.back().values.set(f_v);
      new_domains.back().popcount = 1;
    }
  }
  return new_domains;
}

template <unsigned n_words_>
auto initialise_domains(
    const Model<n_words_> & m,
    Domains<n_words_> & domains) -> bool
{
  /* pattern and target neighbourhood degree sequences */
  vector<vector<vector<int> > > patterns_ndss(m.max_graphs);
  vector<vector<vector<int> > > targets_ndss(m.max_graphs);

  for (int g = 0 ; g < m.max_graphs ; ++g) {
    patterns_ndss.at(g).resize(m.pattern_size);
    targets_ndss.at(g).resize(m.target_size);
  }

  for (int g = 0 ; g < m.max_graphs ; ++g) {
    for (unsigned i = 0 ; i < m.pattern_size ; ++i) {
      auto ni = m.pattern_graph_rows[i * m.max_graphs + g];
      unsigned np = 0;
      for (int j = ni.first_set_bit_from(np) ; j != -1 ; j = ni.first_set_bit_from(np)) {
        ni.unset(j);
        patterns_ndss.at(g).at(i).push_back(m.patterns_degrees.at(g).at(j));
      }
      sort(patterns_ndss.at(g).at(i).begin(), patterns_ndss.at(g).at(i).end(), greater<int>());
    }

    for (unsigned i = 0 ; i < m.target_size ; ++i) {
      auto ni = m.target_graph_rows[i * m.max_graphs + g];
      unsigned np = 0;
      for (int j = ni.first_set_bit_from(np) ; j != -1 ; j = ni.first_set_bit_from(np)) {
        ni.unset(j);
        targets_ndss.at(g).at(i).push_back(m.targets_degrees.at(g).at(j));
      }
      sort(targets_ndss.at(g).at(i).begin(), targets_ndss.at(g).at(i).end(), greater<int>());
    }
  }

  for (unsigned i = 0 ; i < m.pattern_size ; ++i) {
    domains.at(i).v = i;
    domains.at(i).values.unset_all();

    for (unsigned j = 0 ; j < m.target_size ; ++j) {
      bool ok = true;

      for (int g = 0 ; g < m.max_graphs ; ++g) {
        if (m.pattern_graph_rows[i * m.max_graphs + g].test(i) && ! m.target_graph_rows[j * m.max_graphs + g].test(j)) {
          // not ok, loops
          ok = false;
        }
        else if (targets_ndss.at(g).at(j).size() < patterns_ndss.at(g).at(i).size()) {
          // not ok, degrees differ
          ok = false;
        }
        else {
          // full compare of neighbourhood degree sequences
          for (unsigned x = 0 ; ok && x < patterns_ndss.at(g).at(i).size() ; ++x) {
            if (targets_ndss.at(g).at(j).at(x) < patterns_ndss.at(g).at(i).at(x))
              ok = false;
          }
        }

        if (! ok)
          break;
      }

      if (ok)
        domains.at(i).values.set(j);
    }

    domains.at(i).popcount = domains.at(i).values.popcount();
  }

  FixedBitSet<n_words_> domains_union;
  for (auto & d : domains)
    domains_union.union_with(d.values);

  unsigned domains_union_popcount = domains_union.popcount();
  if (domains_union_popcount < unsigned(m.pattern_size))
    return false;

  for (auto & d : domains)
    d.popcount = d.values.popcount();

  return true;
}

template <unsigned n_words_>
auto cheap_all_different(Domains<n_words_> & domains) -> bool
{
  // Pick domains smallest first; ties are broken by smallest .v first.
  // For each popcount p we have a linked list, whose first member is
  // first[p].  The element following x in one of these lists is next[x].
  // Any domain with a popcount greater than domains.size() is put
  // int the "popcount==domains.size()" bucket.
  // The "first" array is sized to be able to hold domains.size()+1
  // elements
  array<int, n_words_ * bits_per_word + 1> first;
  array<int, n_words_ * bits_per_word> next;
  fill(first.begin(), first.begin() + domains.size() + 1, -1);
  fill(next.begin(), next.begin() + domains.size(), -1);
  // Iterate backwards, because we insert elements at the head of
  // lists and we want the sort to be stable
  for (int i = int(domains.size()) - 1 ; i >= 0; --i) {
    unsigned popcount = domains.at(i).popcount;
    if (popcount > domains.size())
      popcount = domains.size();
    next.at(i) = first.at(popcount);
    first.at(popcount) = i;
  }

  // counting all-different
  FixedBitSet<n_words_> domains_so_far, hall;
  unsigned neighbours_so_far = 0;

  for (unsigned i = 0 ; i <= domains.size() ; ++i) {
    // iterate over linked lists
    int domain_index = first[i];
    while (domain_index != -1) {
      auto & d = domains.at(domain_index);

      d.values.intersect_with_complement(hall);
      d.popcount = d.values.popcount();

      if (0 == d.popcount)
        return false;

      domains_so_far.union_with(d.values);
      ++neighbours_so_far;

      unsigned domains_so_far_popcount = domains_so_far.popcount();
      if (domains_so_far_popcount < neighbours_so_far) {
        return false;
      }
      else if (domains_so_far_popcount == neighbours_so_far) {
        // equivalent to hall=domains_so_far
        hall.union_with(domains_so_far);
      }
      domain_index = next[domain_index];
    }
  }

  return true;
}

template <unsigned n_words_>
struct SIPNode {
  Domains<n_words_> domains;
  Assignments assignments;
  bool propagationSuccess;

  bool sat;

  SIPNode() = default;

  SIPNode(Domains<n_words_> domains, Assignments assignments) :
      domains(domains), assignments(assignments), propagationSuccess(true), sat(false) {};
  SIPNode(Domains<n_words_> domains, Assignments assignments, bool propagationSuccess) :
      domains(domains), assignments(assignments), propagationSuccess(propagationSuccess), sat(false) {};

  // If we are SAT then we don't really care about initialising other stuff since it's not used
  SIPNode(Assignments assignments,  bool sat) : assignments(assignments), sat(sat) {};

  bool getObj() const {
    return sat;
  }

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & domains;
    ar & assignments;
    ar & propagationSuccess;
    ar & sat;

  }
};

template <unsigned n_words_>
struct GenNode : YewPar::NodeGenerator<SIPNode<n_words_>, Model<n_words_>> {
  const Domain<n_words_> * branch_domain;

  FixedBitSet<n_words_> remaining;

  array<unsigned, n_words_ * bits_per_word + 1> branch_v;

  unsigned f_v;

  bool sat = false;

  std::reference_wrapper<const Model<n_words_> > model;
  std::reference_wrapper<const SIPNode<n_words_> > parent;

  GenNode(const Model<n_words_> & m, const SIPNode<n_words_> & n) :
    model(std::cref(m)), parent(std::cref(n)) {

    if (!parent.get().propagationSuccess) {
      this->numChildren = 0;
    } else {
      // If we have no more domains then we are done so give a single SAT child
      branch_domain = find_branch_domain(model.get(), parent.get().domains);
      if (!branch_domain) {
        this->numChildren = 1;
        sat = true;
      } else {
        FixedBitSet<n_words_> remaining = branch_domain->values;

        unsigned branch_v_end = 0;
        for (int f_v = remaining.first_set_bit() ; f_v != -1 ; f_v = remaining.first_set_bit()) {
          remaining.unset(f_v);
          branch_v[branch_v_end++] = f_v;
        }

        f_v = 0;
        this->numChildren = branch_v_end;
      }
    }
  }

  // Get the next value
  SIPNode<n_words_> next() override {
    if (sat) { return SIPNode<n_words_>(parent.get().assignments, true); }

    // We need to do the copy in case we are running in parallel
    auto newAssignments = parent.get().assignments;
    Assignment a {branch_domain->v, branch_v[f_v]};
    newAssignments.values.push_back(hpx::util::make_tuple(std::move(a), true));

    auto dom = parent.get().domains;
    auto new_domains = copy_domains_and_assign(dom, branch_domain->v, branch_v[f_v]);

    auto prop = propagate(model.get(), new_domains, newAssignments);

    ++f_v;

    //return SIPNode<n_words_>(std::move(new_domains), std::move(newAssignments), prop);
    return SIPNode<n_words_>(new_domains, std::move(newAssignments), prop);
  }
};

int hpx_main(boost::program_options::variables_map & opts) {
  hpx::cout << "Using pattern file: " << opts["pattern"].as<std::string>() << hpx::endl;
  hpx::cout << "Using target file: " << opts["target"].as<std::string>() << hpx::endl;
  auto patternG = read_lad(opts["pattern"].as<std::string>());
  auto targetG  = read_lad(opts["target"].as<std::string>());

  if (patternG.size() > targetG.size()) {
    std::cerr << "Pattern graph larger than Target graph\n";
    return hpx::finalize();
  }

  const Model<NWORDS> m(targetG, patternG);

  Domains<NWORDS> domains(m.pattern_size);
  if (!initialise_domains(m, domains)) {
    std::cerr << "Could not initialise domains\n";
    return hpx::finalize();
  }

  Assignments assignments;
  assignments.values.reserve(m.pattern_size);

  SIPNode<NWORDS> root(domains, assignments);

  auto sol = root;

  auto start_time = std::chrono::steady_clock::now();

  YewPar::Skeletons::API::Params<bool> searchParameters;
  searchParameters.expectedObjective = true;

  auto skeleton = opts["skeleton"].as<std::string>();
  if (skeleton == "seq") {
    sol = YewPar::Skeletons::Seq<GenNode<NWORDS>,
                                YewPar::Skeletons::API::Decision,
                                YewPar::Skeletons::API::MoreVerbose>
        ::search(m, root, searchParameters);
  } else if (skeleton ==  "depthbounded") {
    searchParameters.spawnDepth = opts["spawn-depth"].as<std::uint64_t>();
    auto poolType = opts["poolType"].as<std::string>();
    if (poolType == "deque") {
      sol = YewPar::Skeletons::DepthBounded<GenNode<NWORDS>,
                                           YewPar::Skeletons::API::Decision,
                                           YewPar::Skeletons::API::DepthBoundedPoolPolicy<
                                             Workstealing::Policies::Workpool>,
                                           YewPar::Skeletons::API::MoreVerbose>
          ::search(m, root, searchParameters);
		} else {
			// EXTENSION
      if (opts.count("nodes")) {
        sol = YewPar::Skeletons::DepthBounded<GenNode<NWORDS>,
                                             YewPar::Skeletons::API::Decision,
                                             YewPar::Skeletons::API::DepthBoundedPoolPolicy<
                                               Workstealing::Policies::DepthPoolPolicy>,
                                             YewPar::Skeletons::API::NodeCounts,
                                             YewPar::Skeletons::API::MoreVerbose>
          ::search(m, root, searchParameters);
      } else if (opts.count("backtracks")) {
				sol = YewPar::Skeletons::DepthBounded<GenNode<NWORDS>,
                                             YewPar::Skeletons::API::Decision,
                                             YewPar::Skeletons::API::DepthBoundedPoolPolicy<
                                               Workstealing::Policies::DepthPoolPolicy>,
                                             YewPar::Skeletons::API::Backtracks,
                                             YewPar::Skeletons::API::MoreVerbose>
          ::search(m, root, searchParameters);
			} else if (opts.count("regularity")) { 
				sol = YewPar::Skeletons::DepthBounded<GenNode<NWORDS>,
                                             YewPar::Skeletons::API::Decision,
                                             YewPar::Skeletons::API::DepthBoundedPoolPolicy<
                                               Workstealing::Policies::DepthPoolPolicy>,
                                             YewPar::Skeletons::API::Regularity,
                                             YewPar::Skeletons::API::MoreVerbose>
          ::search(m, root, searchParameters);
			} else {
        sol = YewPar::Skeletons::DepthBounded<GenNode<NWORDS>,
                                             YewPar::Skeletons::API::Decision,
                                             YewPar::Skeletons::API::DepthBoundedPoolPolicy<
                                               Workstealing::Policies::DepthPoolPolicy>,
                                             YewPar::Skeletons::API::MoreVerbose>
          ::search(m, root, searchParameters);
			}
		}
  } else if (skeleton ==  "stacksteal") {
    searchParameters.stealAll = static_cast<bool>(opts.count("chunked"));
    if (opts.count("nodes")) {
      sol = YewPar::Skeletons::StackStealing<GenNode<NWORDS>,
                                         YewPar::Skeletons::API::Decision,
                                         YewPar::Skeletons::API::MoreVerbose,
                                         YewPar::Skeletons::API::NodeCounts>
        ::search(m, root, searchParameters);
    } else if (opts.count("backtracks")) {
			sol = YewPar::Skeletons::StackStealing<GenNode<NWORDS>,
                                         YewPar::Skeletons::API::Decision,
                                         YewPar::Skeletons::API::MoreVerbose,
                                         YewPar::Skeletons::API::Backtracks>
        ::search(m, root, searchParameters);
		} else if (opts.count("regualrity")) {
			sol = YewPar::Skeletons::StackStealing<GenNode<NWORDS>,
                                         YewPar::Skeletons::API::Decision,
                                         YewPar::Skeletons::API::MoreVerbose,
                                         YewPar::Skeletons::API::Regularity>
        ::search(m, root, searchParameters);
		} else {
      sol = YewPar::Skeletons::StackStealing<GenNode<NWORDS>,
                                         YewPar::Skeletons::API::Decision,
                                         YewPar::Skeletons::API::MoreVerbose>
        ::search(m, root, searchParameters);
    }
  } else if (skeleton ==  "budget") {
    searchParameters.backtrackBudget = opts["backtrack-budget"].as<std::uint64_t>();
    if (opts.count("nodes")) {
      sol = YewPar::Skeletons::Budget<GenNode<NWORDS>,
                                    YewPar::Skeletons::API::Decision,
                                    YewPar::Skeletons::API::NodeCounts,
                                    YewPar::Skeletons::API::MoreVerbose>
        ::search(m, root, searchParameters);
    } else if (opts.count("backtracks")) {
			sol = YewPar::Skeletons::Budget<GenNode<NWORDS>,
                                    YewPar::Skeletons::API::Decision,
                                    YewPar::Skeletons::API::Backtracks,
                                    YewPar::Skeletons::API::MoreVerbose>
        ::search(m, root, searchParameters);
		} else if (opts.count("regularity")) {
			sol = YewPar::Skeletons::Budget<GenNode<NWORDS>,
                                    YewPar::Skeletons::API::Decision,
                                    YewPar::Skeletons::API::Regularity,
                                    YewPar::Skeletons::API::MoreVerbose>
        ::search(m, root, searchParameters);
		} else {
      sol = YewPar::Skeletons::Budget<GenNode<NWORDS>,
                                    YewPar::Skeletons::API::Decision,
                                    YewPar::Skeletons::API::MoreVerbose>
        ::search(m, root, searchParameters);
    }
  } else if (skeleton ==  "ordered") {
    searchParameters.spawnDepth = opts["spawn-depth"].as<std::uint64_t>();
    if (opts.count("discrepancyOrder")) {
      sol = YewPar::Skeletons::Ordered<GenNode<NWORDS>,
                                      YewPar::Skeletons::API::Decision,
                                      YewPar::Skeletons::API::DiscrepancySearch,
                                      YewPar::Skeletons::API::MoreVerbose>
          ::search(m, root, searchParameters);
    } else {
      sol = YewPar::Skeletons::Ordered<GenNode<NWORDS>,
                                      YewPar::Skeletons::API::Decision,
                                      YewPar::Skeletons::API::MoreVerbose>
          ::search(m, root, searchParameters);

    }
  } else {
    std::cerr << "Invalid skeleton type\n";
    return hpx::finalize();
  }

  std::map<int, int> res_isomorphism;
  for (auto & a : sol.assignments.values) {
    res_isomorphism.emplace(m.pattern_permutation.at(get<0>(a).variable),
                            m.target_permutation.at(get<0>(a).value));
  }

  auto overall_time = std::chrono::duration_cast<std::chrono::milliseconds>
      (std::chrono::steady_clock::now() - start_time);

  hpx::cout << "Solution found: " << std::boolalpha << !res_isomorphism.empty() << hpx::endl;
  if (!res_isomorphism.empty()) {
    hpx::cout << "mapping = ";
    for (auto v : res_isomorphism)
      hpx::cout << "(" << v.first << " -> " << v.second << ") ";
    hpx::cout << hpx::endl;
  }

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
        boost::program_options::value<std::uint64_t>()->default_value(0),
        "Backtrack budget for budget skeleton"
      )
      ("poolType",
       boost::program_options::value<std::string>()->default_value("depthpool"),
       "Pool type for depthbounded skeleton")
      ("discrepancyOrder", "Use discrepancy order for the ordered skeleton")
      ("chunked", "Use chunking with stack stealing")
      ("pattern",
      boost::program_options::value<std::string>()->required(),
      "Specify the pattern file (LAD format)"
      )
      ("target",
      boost::program_options::value<std::string>()->required(),
      "Specify the target file (LAD format)"
      )
      ("nodes", "Collect the node throughput info")
			("backtracks", "Collect backtracking info")
			("regularity", "Collect regularity info");

  YewPar::registerPerformanceCounters();

  return hpx::init(desc_commandline, argc, argv);
}
