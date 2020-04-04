#ifndef SKELETONS_API_HPP
#define SKELETONS_API_HPP

#include <boost/parameter.hpp>
#include <boost/serialization/access.hpp>

namespace YewPar { namespace Skeletons {

namespace parameter = boost::parameter;

namespace API {

// Compile time configuration

// Parameters that can be used by simply adding them to the template arg list (value-less)
#define DEF_PRESENT_PARAMETER(name, param)                      \
  BOOST_PARAMETER_TEMPLATE_KEYWORD(param)                       \
  struct name : param<std::integral_constant<bool, true> > {};

// Tree Search Types
DEF_PRESENT_PARAMETER(CountNodes, CountNodes_)
DEF_PRESENT_PARAMETER(Optimisation, Optimisation_)
DEF_PRESENT_PARAMETER(Decision, Decision_)

// Search Shape changers
DEF_PRESENT_PARAMETER(DepthLimited, DepthLimited_)

// Bounding/Pruning
BOOST_PARAMETER_TEMPLATE_KEYWORD(BoundFunction)
BOOST_PARAMETER_TEMPLATE_KEYWORD(ObjectiveComparison)
BOOST_PARAMETER_TEMPLATE_KEYWORD(MaxStackDepth)

// Optimisations
DEF_PRESENT_PARAMETER(PruneLevel, PruneLevel_)

// Depth bounded policies
BOOST_PARAMETER_TEMPLATE_KEYWORD(DepthBoundedPoolPolicy)

// Ordered Discrpancy search toggle
DEF_PRESENT_PARAMETER(DiscrepancySearch, DiscrepancySearch_)

// Verbose output
BOOST_PARAMETER_TEMPLATE_KEYWORD(Verbose_)
// Basic Info
struct Verbose : Verbose_<std::integral_constant<unsigned, 1> > {};
// Basic Info + Updates
struct MoreVerbose : Verbose_<std::integral_constant<unsigned, 2> > {};
// Basic Info + Updates + Parallelism information
struct EvenMoreVerbose : Verbose_<std::integral_constant<unsigned, 3> > {};

// EXTENSION, flag for node throughput
BOOST_PARAMETER_TEMPLATE_KEYWORD(NodeCounts_)
struct NodeCounts : NodeCounts_<std::integral_constant<unsigned, 1> > {};

// EXTENSION, flag for search task runtime regularity
BOOST_PARAMETER_TEMPLATE_KEYWORD(Regularity_)
struct Regularity : Regularity_<std::integral_constant<unsigned, 1> > {};

// EXTENSION, flag for BSW
BOOST_PARAMETER_TEMPLATE_KEYWORD(Backtracks_)
struct Backtracks : Backtracks_<std::integral_constant<unsigned, 1> > {};


// Signature, everything is optional since the generators are explicitly passed as arg 1 on each skeleton
BOOST_PARAMETER_TEMPLATE_KEYWORD(null)
typedef parameter::parameters<parameter::optional<tag::null> > skeleton_signature;

template <typename Obj = bool>
struct Params {
  // For depth limited searches
  unsigned maxDepth = 5000;

  // For decision based problems
  Obj expectedObjective;

  // For B&B
  Obj initialBound = false;

  // Depth Spawns
  unsigned spawnDepth = 1;

  // Stack Steals
  // Should we steal all remaining nodes at the highest depth or just one?
  bool stealAll = false;

  // Budget
  // FIXME: How to determine a good value for this?
  unsigned backtrackBudget = 200;

  // Needed to push to registries on all nodes
  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & maxDepth;
    ar & expectedObjective;
    ar & initialBound;
    ar & spawnDepth;
    ar & stealAll;
    ar & backtrackBudget;
  }
};

}}}

#endif
