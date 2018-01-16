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
DEF_PRESENT_PARAMETER(BnB, BnB_)
DEF_PRESENT_PARAMETER(Decision, Decision_)

// Search Shape changers
DEF_PRESENT_PARAMETER(DepthBounded, DepthBounded_)

// Bounding/Pruning
BOOST_PARAMETER_TEMPLATE_KEYWORD(BoundFunction)
BOOST_PARAMETER_TEMPLATE_KEYWORD(ObjectiveComparison)
BOOST_PARAMETER_TEMPLATE_KEYWORD(MaxStackDepth)

DEF_PRESENT_PARAMETER(PruneLevel, PruneLevel_)

// Signature, everything is optional since the generators are explicitly passed as arg 1 on each skeleton
BOOST_PARAMETER_TEMPLATE_KEYWORD(null)
typedef parameter::parameters<parameter::optional<tag::null> > skeleton_signature;

template <typename Obj = bool>
struct Params {
  friend class boost::serialization::access;

  // For depth limited searches
  unsigned maxDepth;

  // For decision based problems
  Obj expectedObjective;

  // Depth Spawns
  unsigned spawnDepth = 1;

  // Stack Steals
  // Should we steal all remaining nodes at the highest depth or just one?
  bool stealAll = false;

  // Needed to push to registries on all nodes
  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & maxDepth;
    ar & expectedObjective;
    ar & spawnDepth;
  }
};

}}}

#endif
