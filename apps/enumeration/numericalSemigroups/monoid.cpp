#include <iostream>
#include "monoid.hpp"

void init_full_N(Monoid &m)
{
  epi8 block ={1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8};
  for(auto i=0; i<NBLOCKS; i++){
    m.blocks[i] = block;
    block = block + 8;
  }
  m.genus = 0;
  m.conductor = 1;
  m.min = 1;
}

void print_monoid(const Monoid &m)
{
  unsigned int i;
  std::cout<<"min = "<<m.min<<", cond = "<<m.conductor<<", genus = "<<m.genus<<", decs = ";
  for (i=0; i<SIZE; i++) std::cout<<((int) m.decs[i])<<' ';
  std::cout<<std::endl;
}

void print_epi8(epi8 bl)
{
  unsigned int i;
  for (i=0; i<16; i++) std::cout<<((uint8_t*)&bl)[i]<<' ';
  std::cout<<std::endl;
}
