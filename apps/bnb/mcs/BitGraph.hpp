#ifndef _BITGRAPH_HPP_
#define _BITGRAPH_HPP_

/**
 * Bitset-encoded graph, using a fixed number of words which is selected at
 * compile time.
 */

#include "BitSet.hpp"

#include <vector>

template <unsigned n_words_>
class BitGraph
{
private:
  using Rows = std::vector<BitSet<n_words_> >;

  int _size = 0;
  Rows _adjacency;

public:
  auto size() const -> int {
    return _size;
  }

  auto resize(int size) -> void {
    _size = size;
    _adjacency.resize(size);
    for (auto & row : _adjacency)
      row.resize(size);
  }

  auto add_edge(int a, int b) -> void {
    _adjacency[a].set(b);
    _adjacency[b].set(a);
  }

  auto adjacent(int a, int b) const -> bool {
    return _adjacency[a].test(b);
  }

  auto intersect_with_row(int row, BitSet<n_words_> & p) const -> void {
    p.intersect_with(_adjacency[row]);
  }

  auto intersect_with_row_complement(int row, BitSet<n_words_> & p) const -> void {
    p.intersect_with_complement(_adjacency[row]);
  }

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & _size;
    ar & _adjacency;
  }
};

#endif
