#ifndef SKELETONS_API_HPP
#define SKELETONS_API_HPP

#include <boost/parameter.hpp>

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
DEF_PRESENT_PARAMETER(PruneLevel, PruneLevel_)

// Signature, everything is optional since the generators are explicitly passed as arg 1 on each skeleton
BOOST_PARAMETER_TEMPLATE_KEYWORD(null)
typedef parameter::parameters<parameter::optional<tag::null> > skeleton_signature;

template <typename Obj = bool>
struct Params {
  // For depth limited searches
  unsigned maxDepth;

  // For decision based problems
  Obj expectedObjective;
};

}}}

#endif
