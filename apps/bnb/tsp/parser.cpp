#include "parser.hpp"

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

    static const std::regex node { R"(\s*(\d+)\s+(\d+[.\d]*)\s+(\d+[.\d]*))" };
    static const std::regex type { R"(EDGE_WEIGHT_TYPE: (\w+))" };
    std::smatch match;
    if (regex_match(line, match, node)) {
      result.nodeInfo[std::stoi(match.str(1))] = std::make_pair(std::stod(match.str(2)), std::stod(match.str(3)));
      // Assumes the nodes are in increasing order
      result.numNodes = std::stoi(match.str(0));
    } else if (regex_match(line, match, type)) {
      if (match.str(1) == "EUC_2D") result.type = TSP_TYPE::EUC_2D;
      if (match.str(1) == "GEO")    result.type = TSP_TYPE::GEO;
    }
  }

  if (! infile.eof()) {
    throw SomethingWentWrong { "Error reading file" };
  }

  return result;
}

// From https://www.iwr.uni-heidelberg.de/groups/comopt/software/TSPLIB95/TSPFAQ.html
std::pair<double, double> getLatLon(std::pair<double, double> p) {
  auto conv = [](double x) {
    auto PI = 3.141592;
    auto deg = (int) x;
    auto min = x - deg;
    return PI * (deg + 5.0 * min/ 3.0) / 180.0;
  };

  return std::make_pair(conv(p.first), conv(p.second));
}

unsigned calculateDistanceLatLon(std::pair<double, double> n1, std::pair<double, double> n2) {
  double lat1, lat2, lon1, lon2;
  std::tie(lat1, lon1) = getLatLon(n1);
  std::tie(lat2, lon2) = getLatLon(n2);

  auto RRR = 6378.388;

  auto q1 = std::cos( lon1  - lon2 );
  auto q2 = std::cos( lat1 - lat2 );
  auto q3 = std::cos( lat1 + lat2 );
  return (int) ( RRR * std::acos( 0.5*((1.0+q1)*q2 - (1.0-q1)*q3) ) + 1.0);
}
