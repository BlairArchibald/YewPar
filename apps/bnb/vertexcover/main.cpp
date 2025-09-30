#include <iostream>
#include <numeric>
#include <algorithm>
#include <vector>
#include <map>
#include <chrono>

#include <hpx/hpx_init.hpp>
#include <hpx/iostream.hpp>

#include <boost/serialization/access.hpp>

#include "../maxclique/BitGraph.hpp"
#include "../maxclique/BitSet.hpp"
#include "../maxclique/DimacsParser.hpp"

#include "YewPar.hpp"

#include "skeletons/Seq.hpp"
#include "skeletons/DepthBounded.hpp"
#include "skeletons/StackStealing.hpp"
#include "skeletons/Ordered.hpp"
#include "skeletons/Budget.hpp"

#include "util/func.hpp"
#include "util/NodeGenerator.hpp"

// Number of Words to use in our bitset representation
#ifndef NWORDS
#define NWORDS 8
#endif

// graph production

// vertec cover solution state

// node generator

// bounding function

// hpx main
int hpx_main(hpx::program_options::variables_map& opts) {
    return hpx::finalize();
}
// main program
int main(int argc, char* argv[]) {
  return hpx::init(argc, argv);
}