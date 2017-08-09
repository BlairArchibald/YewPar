#ifndef VF_PARSER_HPP
#define VF_PARSER_HPP

#include <string>
#include <vector>

class GraphFileError : public std::exception {
private:
  std::string _what;

public:
  GraphFileError(const std::string & filename, const std::string & message) throw ();

  auto what() const throw () -> const char *;
};

struct VFGraph {
  unsigned size;
  std::vector<unsigned> vertex_labels;
  std::vector<std::vector<unsigned> > vertices_by_label;
  std::vector<std::vector<unsigned> > edges;
};

auto read_vf(const std::string & filename, bool unlabelled, bool no_edge_labels, bool undirected) -> VFGraph;

#endif
