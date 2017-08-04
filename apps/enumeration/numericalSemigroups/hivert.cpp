#include <hpx/hpx.hpp>
#include <hpx/hpx_init.hpp>

#include "enumerate/dist.hpp"
#include "enumerate/macros.hpp"
#include "enumerate/nodegenerator.hpp"

#include "monoid.hpp"

// Numerical Semigroups don't have a space
struct Empty {};

struct NodeGen : skeletons::Enum::NodeGenerator<Empty, Monoid> {
  Monoid group;
  generator_iter<CHILDREN> it;

  NodeGen() : it(generator_iter<CHILDREN>(Monoid())) {
    this->numChildren = 0;
  }

  NodeGen(const Monoid & s) : group(s), it(generator_iter<CHILDREN>(s)){
    this->numChildren = it.count();
    it.move_next(); // Original code skips begin
  }

  Monoid next(const Empty & space, const Monoid & n) override {
    auto res = remove_generator(group, it.get_gen());
    it.move_next();
    return res;
  }
};

NodeGen generateChildren(const Empty & space, const Monoid & s) {
  return NodeGen(s);
}

HPX_PLAIN_ACTION(generateChildren, gen_action)
YEWPAR_CREATE_ENUM_ACTION(childTask_act, Empty, Monoid, gen_action)
REGISTER_ENUM_REGISTRY(Empty, Monoid)

int hpx_main(boost::program_options::variables_map & opts) {
  auto spawnDepth = opts["spawn-depth"].as<unsigned>();
  auto maxDepth   = opts["until-depth"].as<unsigned>();

  Monoid root;
  init_full_N(root);

  auto counts = skeletons::Enum::Dist::count<Empty, Monoid, gen_action, childTask_act>(spawnDepth, maxDepth, Empty(), root);

  std::cout << "Results Table: " << std::endl;
  for (const auto & elem : counts) {
    std::cout << elem.first << ": " << elem.second << std::endl;
  }
  std::cout << "=====" << std::endl;

  return hpx::finalize();
}

int main(int argc, char* argv[]) {
  boost::program_options::options_description
    desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

  desc_commandline.add_options()
    ( "spawn-depth,s",
      boost::program_options::value<unsigned>()->default_value(0),
      "Depth in the tree to spawn until (for parallel skeletons only)"
    )
    ( "until-depth,d",
      boost::program_options::value<unsigned>()->default_value(0),
      "Depth in the tree to count until"
    );

  hpx::register_startup_function(&workstealing::registerPerformanceCounters);

  return hpx::init(desc_commandline, argc, argv);
}
