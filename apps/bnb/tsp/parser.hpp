#ifndef TSP_PARSER
#define TSP_PARSER

#include <exception>
#include <unordered_map>
#include <fstream>
#include <regex>
#include <cmath>

enum TSP_TYPE {
  EUC_2D, GEO
};

class SomethingWentWrong : public std::exception {
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

TSPFromFile parseFile (const std::string & filename);
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

  return costs;
}

unsigned calculateDistanceLatLon(std::pair<double, double> n1, std::pair<double, double> n2);

template<unsigned N>
DistanceMatrix<N> buildDistanceMatrixGEO (const TSPFromFile & data) {
  DistanceMatrix<N> costs;

  for (const auto & n1 : data.nodeInfo) {
    for (const auto & n2 : data.nodeInfo) {
      if (n1.first == n2.first) {
        costs[n1.first][n2.first] = 0;
      } else {
        costs[n1.first][n2.first] = calculateDistanceLatLon(n1.second, n2.second);
      }
    }
  }

  return costs;
}

#endif
