#ifndef _BITSET_HPP_
#define _BITSET_HPP_

/**
 * Ciaran McCressh's Bitset implementation for MaxClique, containing a fixed
 * number of words which is selected at compile time.
 */

#include <array>

using BitWord = unsigned long long;
static const constexpr int bits_per_word = sizeof(BitWord) * 8;

template <unsigned words_>
class BitSet {
private:
  using Bits = std::array<BitWord, words_>;

  int _size  = 0;
  Bits _bits = {{ }};

public:
  auto resize(int size) -> void {
    _size = size;
  }

  auto set(int a) -> void {
    _bits[a / bits_per_word] |= (BitWord{ 1 } << (a % bits_per_word));
  }

  auto unset(int a) -> void {
    _bits[a / bits_per_word] &= ~(BitWord{ 1 } << (a % bits_per_word));
  }

  auto set_all() -> void {
    /* only done once, not worth making it clever */
    for (int i = 0 ; i < _size ; ++i)
      set(i);
  }

  auto test(int a) const -> bool {
    return _bits[a / bits_per_word] & (BitWord{ 1 } << (a % bits_per_word));
  }

  auto popcount() const -> unsigned {
    unsigned result = 0;
    for (auto & p : _bits)
      result += __builtin_popcountll(p);
    return result;
  }

  auto empty() const -> bool {
    for (auto & p : _bits)
      if (0 != p)
        return false;
    return true;
  }

  auto intersect_with(const BitSet<words_> & other) -> void {
    for (typename Bits::size_type i = 0 ; i < words_ ; ++i)
      _bits[i] = _bits[i] & other._bits[i];
  }

  auto intersect_with_complement(const BitSet<words_> & other) -> void {
    for (typename Bits::size_type i = 0 ; i < words_ ; ++i)
      _bits[i] = _bits[i] & ~other._bits[i];
  }

  auto first_set_bit() const -> int {
    for (typename Bits::size_type i = 0 ; i < _bits.size() ; ++i) {
      int b = __builtin_ffsll(_bits[i]);
      if (0 != b)
        return i * bits_per_word + b - 1;
    }
    return -1;
  }

  template<class Archive>
  void serialize(Archive & ar, const unsigned version) {
    ar & _size;
    ar & _bits;
  }
};

#endif
