#include "VFParser.hpp"

#include <fstream>
#include <string>

GraphFileError::GraphFileError(const std::string & filename, const std::string & message) throw () :
  _what("Error reading graph file '" + filename + "': " + message) {}

auto GraphFileError::what() const throw () -> const char * {
  return _what.c_str();
}

auto read_word(std::ifstream & infile) -> unsigned
{
  unsigned char a, b;
  a = static_cast<unsigned char>(infile.get());
  b = static_cast<unsigned char>(infile.get());
  return unsigned(a) | (unsigned(b) << 8);
}

auto read_vf(const std::string & filename, bool unlabelled, bool no_edge_labels, bool undirected) -> VFGraph
{
  VFGraph result;

  std::ifstream infile{ filename };
  if (! infile)
    throw GraphFileError{ filename, "unable to open file" };

  result.size = read_word(infile);
  if (! infile)
    throw GraphFileError{ filename, "error reading size" };

  // to be like the CP 2011 labelling scheme...
  int m = result.size * 33 / 100;
  int p = 1, k1 = 0, k2 = 0;
  while (p < m && k1 < 16) {
    p *= 2;
    k1 = k2;
    k2++;
  }

  if (unlabelled)
    result.vertices_by_label.resize(1);
  else
    result.vertices_by_label.resize(0x10000 >> (16 - k1));

  result.vertex_labels.resize(result.size);
  result.edges.resize(result.size);
  for (auto & e : result.edges)
    e.resize(result.size);

  for (unsigned r = 0 ; r < result.size ; ++r) {
    unsigned l = read_word(infile) >> (16 - k1);
    if (unlabelled)
      l = 0;
    result.vertex_labels.at(r) = l;
    result.vertices_by_label.at(l).push_back(r);
  }

  if (! infile)
    throw GraphFileError{ filename, "error reading attributes" };

  for (unsigned r = 0 ; r < result.size ; ++r) {
    int c_end = read_word(infile);
    if (! infile)
      throw GraphFileError{ filename, "error reading edges count" };

    for (int c = 0 ; c < c_end ; ++c) {
      unsigned e = read_word(infile);

      if (e >= result.size)
        throw GraphFileError{ filename, "edge index " + std::to_string(e) + " out of bounds" };

      if (unlabelled) {
        result.edges[r][e] = 1;
        read_word(infile);
      }
      else {
        unsigned l = (read_word(infile) >> (16 - k1)) + 1;
        //                     if (result.edges[r][e] != 0 && result.edges[r][e] != l)
        //                         throw GraphFileError{ filename, "contradicting labels on " + std::to_string(r) + " and " + std::to_string(e) };

        if (no_edge_labels)
          l = 1;
        result.edges[r][e] = l;
      }

      if (undirected) {
        //                    if (result.edges[e][r] != 0 && result.edges[e][r] != result.edges[r][e])
        //                        throw GraphFileError{ filename, "contradicting labels on " + std::to_string(r) + " and " + std::to_string(e) };
        result.edges.at(e).at(r) = result.edges.at(r).at(e);
      }
    }
  }

  infile.peek();
  if (! infile.eof())
    throw GraphFileError{ filename, "EOF not reached" };

  return result;
}
