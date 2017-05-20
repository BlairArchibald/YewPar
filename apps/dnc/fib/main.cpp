#include <hpx/hpx.hpp>
#include <hpx/hpx_init.hpp>

#include <utility>

#include "dnc/dnc-seq.hpp"
#include "dnc/dnc-par.hpp"
#include "dnc/dnc-dist.hpp"
#include "dnc/macros.hpp"

// Fib functionality
std::uint64_t fib(std::uint64_t n) {
  return n <= 2 ? 1 : fib(n - 1) + fib(n - 2);
}

bool fib_trivial(std::uint64_t n) {
  return n < 30;
}

std::pair<std::uint64_t, std::uint64_t> fib_divide(std::uint64_t n) {
  return std::make_pair(n - 1, n - 2);
}

std::uint64_t fib_conquer(std::uint64_t n1, std::uint64_t n2) {
  return n1 + n2;
}

// Required Action definitions
HPX_PLAIN_ACTION(fib, fib_action)
HPX_PLAIN_ACTION(fib_trivial, fib_trivial_action)
HPX_PLAIN_ACTION(fib_divide, fib_divide_action)
HPX_PLAIN_ACTION(fib_conquer, fib_conquer_action)
YEWPAR_CREATE_DNC_PAR_ACTION(fib_par_act, std::uint64_t, fib_divide_action, fib_conquer_action, fib_trivial_action, fib_action)
YEWPAR_CREATE_DNC_DIST_ACTION(fib_dist_act, std::uint64_t, fib_divide_action, fib_conquer_action, fib_trivial_action, fib_action)


int hpx_main(boost::program_options::variables_map & opts) {
  const std::vector<std::string> skeletonTypes = {"seq", "par", "dist" };

  auto skeletonType = opts["skeleton"].as<std::string>();
  auto found = std::find(std::begin(skeletonTypes), std::end(skeletonTypes), skeletonType);
  if (found == std::end(skeletonTypes)) {
    std::cout << "Invalid skeleton type option. Should be: seq, par or dist" << std::endl;
    hpx::finalize();
    return EXIT_FAILURE;
  }

  auto n = opts["n"].as<std::uint64_t>();

  std::uint64_t res = 0;

  if (skeletonType == "seq") {
    res = skeletons::DnC::Seq::dnc<std::uint64_t>
      (fib_divide, fib_conquer, fib_trivial, fib, n);
  }

  if (skeletonType == "par") {
    res = skeletons::DnC::Par::dnc<std::uint64_t, fib_divide_action, fib_conquer_action, fib_trivial_action, fib_action, fib_par_act>
      (n);
  }

  if (skeletonType == "dist") {
    res = skeletons::DnC::Dist::dnc<std::uint64_t, fib_divide_action, fib_conquer_action, fib_trivial_action, fib_action, fib_dist_act>
      (n);
  }

  std::cout << "Fib(" << n << ") = " << res << std::endl;

  return hpx::finalize();
}

int main(int argc, char* argv[]) {
  boost::program_options::options_description
    desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

  desc_commandline.add_options()
    ( "n",
      boost::program_options::value<std::uint64_t>()->default_value(30),
      "Value to calculate fib for"
      )
    ( "skeleton",
      boost::program_options::value<std::string>()->default_value("seq"),
      "Type of skeleton to use: seq, par, dist"
      );

  return hpx::init(desc_commandline, argc, argv);
}
