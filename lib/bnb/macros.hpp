#ifndef YEWPAR_MACROS_HPP
#define YEWPAR_MACROS_HPP

#define COMMA ,

#define YEWPAR_CREATE_BNB_PAR_ACTION(name, space, sol, bnd, cands, genf, bndf)                                              \
  struct name : hpx::actions::make_action<                                                                                  \
  decltype(&skeletons::BnB::Par::searchChildTask<space COMMA sol COMMA bnd COMMA cands COMMA genf COMMA bndf COMMA name>),  \
  &skeletons::BnB::Par::searchChildTask<space COMMA sol COMMA bnd COMMA cands COMMA genf COMMA bndf COMMA name>,            \
  name>::type {};                                                                                                           \
  HPX_REGISTER_ACTION_DECLARATION(name, name);                                                                              \
  HPX_REGISTER_ACTION(name, name);                                                                                          \

#define YEWPAR_CREATE_BNB_DIST_ACTION(name, space, sol, bnd, cands, genf, bndf)                                             \
  struct name : hpx::actions::make_action<                                                                                  \
  decltype(&skeletons::BnB::Dist::searchChildTask<space COMMA sol COMMA bnd COMMA cands COMMA genf COMMA bndf COMMA name>), \
  &skeletons::BnB::Dist::searchChildTask<space COMMA sol COMMA bnd COMMA cands COMMA genf COMMA bndf COMMA name>,           \
  name>::type {};                                                                                                           \
  HPX_REGISTER_ACTION_DECLARATION(name, name);                                                                              \
  HPX_REGISTER_ACTION(name, name);                                                                                          \

#endif
