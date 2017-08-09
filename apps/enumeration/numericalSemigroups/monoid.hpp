#ifndef MONOID_HPP
#define MONOID_HPP

#include <cstdint>
#include <array>

// We cant have those as C++ constant because of the #define used below in
// remove_generator manual loop unrolling. I don't know if the unrolling is
// doable using template metaprogamming. I didn't manage to figure out how.

#ifndef MAX_GENUS
#error "Please define the MAX_GENUS macro"
#endif
#define SIZE_BOUND (3*(MAX_GENUS-1))
#define NBLOCKS ((SIZE_BOUND+15) >> 4)
#define SIZE (NBLOCKS << 4)

#include <x86intrin.h>

typedef uint8_t epi8 __attribute__ ((vector_size (16)));
typedef uint8_t dec_numbers[SIZE] __attribute__ ((aligned (16)));
typedef std::array<epi8, NBLOCKS> dec_blocks;
typedef uint_fast64_t ind_t;  // The type used for array indexes

// We need to specialise an instance to epi8 since boost doesn't automatically know how to
// handle the __vector(16) attr
namespace hpx { namespace serialization {
  template<class Archive>
  void serialize(Archive & ar, epi8 & x, const unsigned int version) {
    ar & x;
  }
}}

struct Monoid
{
  union {
    dec_numbers decs;
    dec_blocks blocks;
  };
  // Dont use char as they have to be promoted to 64 bits to do pointer arithmetic.
  ind_t conductor, min, genus;

  template <class Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & decs;
    ar & conductor;
    ar & min;
    ar & genus;
  }
};

void init_full_N(Monoid &);
void print_monoid(const Monoid &);
void print_epi8(epi8);
inline void copy_blocks(dec_blocks &__restrict__ dst,
			const dec_blocks &__restrict__ src);
inline void remove_generator(Monoid &__restrict__ dst,
		      const Monoid &__restrict__ src,
		      ind_t gen);
inline Monoid remove_generator(const Monoid &src, ind_t gen);


// typedef enum { ALL, CHILDREN } generator_type;
class ALL {};
class CHILDREN {};

// template <generator_type T> class generator_iter
template <class T> class generator_iter
{
private:

  const Monoid &m;
  unsigned int mask;   // movemask_epi8 returns a 32 bits values
  ind_t iblock, gen, bound;

public:

  generator_iter(const Monoid &mon);
  bool move_next();
  uint8_t count();
  inline ind_t get_gen() const {return gen; };
};



///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
////////////////   Implementation part here for inlining   ////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

/*
Note: for some reason the code using gcc vector variables is 2-3% faster than
the code using gcc intrinsic instructions.
Here are the results of two runs:
data_mmx =  [9.757, 9.774, 9.757, 9.761, 9.811, 9.819, 9.765, 9.888, 9.774, 9.771]
data_epi8 = [9.592, 9.535, 9.657, 9.468, 9.460, 9.679, 9.458, 9.461, 9.629, 9.474]
*/

extern inline epi8 load_unaligned_epi8(const uint8_t *t)
{ return (epi8) _mm_loadu_si128((__m128i *) (t)); };

extern inline epi8 shuffle_epi8(epi8 b, epi8 sh)    // Require SSE 3
{ return (epi8) _mm_shuffle_epi8((__m128i) b, (__m128i) sh);}

extern inline int movemask_epi8(epi8 b)
{ return _mm_movemask_epi8((__m128i) b);};

const epi8 zero   = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
const epi8 block1 = zero + 1;
const uint8_t m1 = 255;
const epi8 shift16[16] =
  { { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15},
    {m1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14},
    {m1,m1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13},
    {m1,m1,m1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12},
    {m1,m1,m1,m1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11},
    {m1,m1,m1,m1,m1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10},
    {m1,m1,m1,m1,m1,m1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
    {m1,m1,m1,m1,m1,m1,m1, 0, 1, 2, 3, 4, 5, 6, 7, 8},
    {m1,m1,m1,m1,m1,m1,m1,m1, 0, 1, 2, 3, 4, 5, 6, 7},
    {m1,m1,m1,m1,m1,m1,m1,m1,m1, 0, 1, 2, 3, 4, 5, 6},
    {m1,m1,m1,m1,m1,m1,m1,m1,m1,m1, 0, 1, 2, 3, 4, 5},
    {m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1, 0, 1, 2, 3, 4},
    {m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1, 0, 1, 2, 3},
    {m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1, 0, 1, 2},
    {m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1, 0, 1},
    {m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1, 0} };
const epi8 mask16[16] =
  { {m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1},
    { 0,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1},
    { 0, 0,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1},
    { 0, 0, 0,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1},
    { 0, 0, 0, 0,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1},
    { 0, 0, 0, 0, 0,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1},
    { 0, 0, 0, 0, 0, 0,m1,m1,m1,m1,m1,m1,m1,m1,m1,m1},
    { 0, 0, 0, 0, 0, 0, 0,m1,m1,m1,m1,m1,m1,m1,m1,m1},
    { 0, 0, 0, 0, 0, 0, 0, 0,m1,m1,m1,m1,m1,m1,m1,m1},
    { 0, 0, 0, 0, 0, 0, 0, 0, 0,m1,m1,m1,m1,m1,m1,m1},
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,m1,m1,m1,m1,m1,m1},
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,m1,m1,m1,m1,m1},
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,m1,m1,m1,m1},
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,m1,m1,m1},
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,m1,m1},
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,m1} };


template<> inline generator_iter<ALL>::generator_iter(const Monoid &mon)
  : m(mon), bound((mon.conductor+mon.min+15) >> 4)
{
  epi8 block;
  iblock = 0;
  block = m.blocks[0];
  mask  = movemask_epi8(block == block1);
  mask &= 0xFFFE; // 0 is not a generator
  gen = - 1;
};

template<> inline generator_iter<CHILDREN>::generator_iter(const Monoid &mon)
  : m(mon), bound((mon.conductor+mon.min+15) >> 4)
{
  epi8 block;
  iblock = m.conductor >> 4;
  block = m.blocks[iblock] & mask16[m.conductor & 0xF];
  mask  = movemask_epi8(block == block1);
  gen = (iblock << 4) - 1;
};

template <class T> inline uint8_t generator_iter<T>::count()
{
  uint8_t nbr = _mm_popcnt_u32(mask); // popcnt returns a 8 bits value
  for (ind_t ib = iblock+1; ib < bound; ib++)
    nbr += _mm_popcnt_u32(movemask_epi8(m.blocks[ib] == block1));
  return nbr;
};

template <class T> inline bool generator_iter<T>::move_next()
{
  while (!mask)
    {
      iblock++;
      if (iblock > bound) return false;
      gen = (iblock << 4) - 1;
      mask  = movemask_epi8(m.blocks[iblock] == block1);
    }
  unsigned char shift = __bsfd (mask) + 1; // Bit Scan Forward
  gen += shift;
  mask >>= shift;
  return true;
};



inline __attribute__((always_inline))
void copy_blocks(dec_blocks &dst, dec_blocks const &src)
{
  for (ind_t i=0; i<NBLOCKS; i++) dst[i] = src[i];
}


#include <cassert>

inline __attribute__((always_inline))
void remove_generator(Monoid &__restrict__ dst,
		      const Monoid &__restrict__ src,
		      ind_t gen)
{
  ind_t start_block, decal;
  epi8 block;

  assert(src.decs[gen] == 1);

  dst.conductor = gen + 1;
  dst.genus = src.genus + 1;
  dst.min = (gen == src.min) ? dst.conductor : src.min;

  copy_blocks(dst.blocks, src.blocks);

  start_block = gen >> 4;
  decal = gen & 0xF;
  // Shift block by decal uchar
  block = shuffle_epi8(src.blocks[0], shift16[decal]);
  dst.blocks[start_block] -= ((block != zero) & block1);
#if NBLOCKS >= 5

#define CASE_UNROLL(i_loop)			\
  case i_loop : \
    dst.blocks[i_loop+1] -= (load_unaligned_epi8(srcblock) != zero) & block1; \
    srcblock += sizeof(epi8);

  {
    const uint8_t *srcblock = src.decs + sizeof(epi8) - decal;
  switch(start_block)
    {
      CASE_UNROLL(0);
#if NBLOCKS > 2
      CASE_UNROLL(1);
#endif
#if NBLOCKS > 3
      CASE_UNROLL(2);
#endif
#if NBLOCKS > 4
      CASE_UNROLL(3);
#endif
#if NBLOCKS > 5
      CASE_UNROLL(4);
#endif
#if NBLOCKS > 6
      CASE_UNROLL(5);
#endif
#if NBLOCKS > 7
      CASE_UNROLL(6);
#endif
#if NBLOCKS > 8
      CASE_UNROLL(7);
#endif
#if NBLOCKS > 9
      CASE_UNROLL(8);
#endif
#if NBLOCKS > 10
      CASE_UNROLL(9);
#endif
#if NBLOCKS > 11
      CASE_UNROLL(10);
#endif
#if NBLOCKS > 12
      CASE_UNROLL(11);
#endif
#if NBLOCKS > 13
      CASE_UNROLL(12);
#endif
#if NBLOCKS > 14
      CASE_UNROLL(13);
#endif
#if NBLOCKS > 15
      CASE_UNROLL(14);
#endif
#if NBLOCKS > 16
#error "Too many blocks"
#endif
    }
  }
#else
#warning "Loop not unrolled"

  for (auto i=start_block+1; i<NBLOCKS; i++)
    {
      // The following won't work due to some alignment problem (bug in GCC 4.7.1?)
      // block = *((epi8*)(src.decs + ((i-start_block)<<4) - decal));
      block = load_unaligned_epi8(src.decs + ((i-start_block)<<4) - decal);
      dst.blocks[i] -= ((block != zero) & block1);
    }
#endif

  assert(dst.decs[dst.conductor-1] == 0);
}

inline Monoid remove_generator(const Monoid &src, ind_t gen)
{
  Monoid dst;
  remove_generator(dst, src, gen);
  return dst;
}

#endif
