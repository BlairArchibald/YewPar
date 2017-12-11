#include <iostream>
#include <fstream>
#include <regex>
#include <cmath>

#include <unordered_map>

#include <hpx/hpx_init.hpp>

#define MAX_CITIES  128

enum TSP_TYPE {
  EUC_2D, GEO
};

class SomethingWentWrong : public std::exception
{
 private:
  std::string _what;

 public:
  SomethingWentWrong(const std::string & message) throw () :
      _what(message) {}

  auto what() const throw () -> const char * {
    return _what.c_str();
  }
};

struct TSPFromFile {
  TSP_TYPE type;
  std::unordered_map<unsigned, std::pair<double, double>> nodeInfo;
  unsigned numNodes;
};

// Very simple, un-robust parser
TSPFromFile parseFile (const std::string & filename) {
  TSPFromFile result;

  std::ifstream infile{ filename };
  if (!infile) {
    throw SomethingWentWrong { "Can't read from file" };
  }

  std::string line;
  while (std::getline(infile, line)) {
    if (line.empty()) continue;

    // TODO: need to parse the type field

    static const std::regex node { R"((\d+) (\d+) (\d+))" };
    static const std::regex type { R"(EDGE_WEIGHT_TYPE: (\w+)" };
    std::smatch match;
    if (regex_match(line, match, node)) {
      result.nodeInfo[std::stoi(match.str(0))] = std::make_pair(std::stod(match.str(1)), std::stod(match.str(2)));
      // Assumes the nodes are in increasing order
      result.numNodes = std::stoi(match.str(0));
    } else if (regex_match(line, match, type)) {
      if (match.str(0) == "EUC_2D") result.type = TSP_TYPE::EUC_2D;
      if (match.str(0) == "GEO")    result.type = TSP_TYPE::GEO;
    }
  }

  if (! infile.eof()) {
    throw SomethingWentWrong { "Error reading file" };
  }

  return result;
}

template<unsigned N>
using DistanceMatrix = std::array<std::array<unsigned, N>, N>;

template<unsigned N>
DistanceMatrix<N> buildDistanceMatrixEUC2D (const TSPFromFile & data) {
  DistanceMatrix<N> costs;
  for (const auto & n1 : data.nodeInfo) {
    for (const auto & n2 : data.nodeInfo) {
      if (n1.first == n2.first) {
        costs[n1.first][n2.first] = 0;
      } else {
        auto x = std::pow((n1.second.first  - n2.second.first), 2);
        auto y = std::pow((n1.second.second - n2.second.second), 2);
        costs[n1.first][n2.first] = std::round(std::sqrt(x + y));
      }
    }
  }
}

template<unsigned N>
DistanceMatrix<N> buildDistanceMatrixGEO (const TSPFromFile & data) {
  DistanceMatrix<N> costs;

  auto getLatLon = [](double x) {
    auto deg = static_cast<double>(std::round(x));
    auto min = x - deg;
    return M_PI * (deg + 5.0 * min / 3.0) / 180.0;
  };

  // TODO: Check logic here
  auto getDistance = [&](auto n1, auto n2) {
    auto q1 = std::cos(getLatLon(n1.second - n2.second));
    auto q2 = std::cos(getLatLon(n1.first - n2.first));
    auto q3 = std::cos(getLatLon(n1.first + n2.first));
    auto res = 6378.388 * std::acos(0.5 * (( 1.0 + q1) * q2 - (1.0 - q1) * q3)) + 1.0;
    return std::floor(res);
  };

  for (const auto & n1 : data.nodeInfo) {
    for (const auto & n2 : data.nodeInfo) {
      if (n1.first == n2.first) {
        costs[n1.first][n2.first] = 0;
      } else {
        costs[n1.first][n2.first] = getDistance(n1.second, n2.second);
      }
    }
  }
}

int hpx_main(boost::program_options::variables_map & opts) {
  auto inputFile = opts["input-file"].as<std::string>();

  TSPFromFile inputData;
  try {
    inputData = parseFile(inputFile);
  } catch (SomethingWentWrong & e) {
    std::cerr << e.what() << std::endl;
    hpx::finalize();
  }

  return hpx::finalize();
}

int main(int argc, char* argv[]) {
  boost::program_options::options_description
      desc_commandline("Usage: " HPX_APPLICATION_STRING " [options]");

  desc_commandline.add_options()
      ( "spawn-depth,d",
        boost::program_options::value<std::uint64_t>()->default_value(0),
        "Depth in the tree to spawn until (for parallel skeletons only)"
        )
      ( "input-file,f",
        boost::program_options::value<std::string>(),
        "Input problem"
        )
      ( "skeleton",
        boost::program_options::value<std::string>()->default_value("seq"),
        "Type of skeleton to use: seq, dist"
        );

  return hpx::init(desc_commandline, argc, argv);
}
