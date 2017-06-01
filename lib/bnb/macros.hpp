#ifndef YEWPAR_MACROS_HPP
#define YEWPAR_MACROS_HPP

#define COMMA ,

#define YEWPAR_CREATE_BNB_PAR_ACTION(...)                                                                                               \
  HPX_UTIL_EXPAND_(BOOST_PP_CAT(YEWPAR_CREATE_BNB_PAR_ACTION_, HPX_UTIL_PP_NARG(__VA_ARGS__)))(__VA_ARGS__)                             \

#define YEWPAR_CREATE_BNB_DECISION_PAR_ACTION(...)                                                                                      \
  HPX_UTIL_EXPAND_(BOOST_PP_CAT(YEWPAR_CREATE_BNB_DECISION_PAR_ACTION_, HPX_UTIL_PP_NARG(__VA_ARGS__)))(__VA_ARGS__)                    \

#define YEWPAR_CREATE_BNB_DIST_ACTION(...)                                                                                              \
  HPX_UTIL_EXPAND_(BOOST_PP_CAT(YEWPAR_CREATE_BNB_DIST_ACTION_, HPX_UTIL_PP_NARG(__VA_ARGS__)))(__VA_ARGS__)                            \

#define YEWPAR_CREATE_BNB_PAR_ACTION_7(name, space, sol, bnd, cands, genf, bndf)                                                        \
  struct name : hpx::actions::make_action<                                                                                              \
  decltype(&skeletons::BnB::Par::searchChildTask<space COMMA sol COMMA bnd COMMA cands COMMA genf COMMA bndf COMMA name>),              \
  &skeletons::BnB::Par::searchChildTask<space COMMA sol COMMA bnd COMMA cands COMMA genf COMMA bndf COMMA name>,                        \
  name>::type {};                                                                                                                       \
  HPX_REGISTER_ACTION_DECLARATION(name, name);                                                                                          \
  HPX_REGISTER_ACTION(name, name);                                                                                                      \

#define YEWPAR_CREATE_BNB_PAR_ACTION_8(name, space, sol, bnd, cands, genf, bndf, prune)                                                 \
  struct name : hpx::actions::make_action<                                                                                              \
  decltype(&skeletons::BnB::Par::searchChildTask<space COMMA sol COMMA bnd COMMA cands COMMA genf COMMA bndf COMMA name COMMA prune>),  \
  &skeletons::BnB::Par::searchChildTask<space COMMA sol COMMA bnd COMMA cands COMMA genf COMMA bndf COMMA name COMMA prune>,            \
  name>::type {};                                                                                                                       \
  HPX_REGISTER_ACTION_DECLARATION(name, name);                                                                                          \
  HPX_REGISTER_ACTION(name, name);                                                                                                      \

#define YEWPAR_CREATE_BNB_DECISION_PAR_ACTION_7(name, space, sol, bnd, cands, genf, bndf)                                               \
  struct name : hpx::actions::make_action<                                                                                              \
  decltype(&skeletons::BnB::Decision::Par::expand<space COMMA sol COMMA bnd COMMA cands COMMA genf COMMA bndf COMMA name>),             \
  &skeletons::BnB::Decision::Par::expand<space COMMA sol COMMA bnd COMMA cands COMMA genf COMMA bndf COMMA name>,                       \
  name>::type {};                                                                                                                       \
  HPX_REGISTER_ACTION_DECLARATION(name, name);                                                                                          \
  HPX_REGISTER_ACTION(name, name);                                                                                                      \

#define YEWPAR_CREATE_BNB_DECISION_PAR_ACTION_8(name, space, sol, bnd, cands, genf, bndf, prune)                                        \
  struct name : hpx::actions::make_action<                                                                                              \
  decltype(&skeletons::BnB::Decision::Par::expand<space COMMA sol COMMA bnd COMMA cands COMMA genf COMMA bndf COMMA name COMMA prune>), \
  &skeletons::BnB::Decision::Par::expand<space COMMA sol COMMA bnd COMMA cands COMMA genf COMMA bndf COMMA name COMMA prune>,           \
  name>::type {};                                                                                                                       \
  HPX_REGISTER_ACTION_DECLARATION(name, name);                                                                                          \
  HPX_REGISTER_ACTION(name, name);                                                                                                      \

#define YEWPAR_CREATE_BNB_DIST_ACTION_7(name, space, sol, bnd, cands, genf, bndf)                                                       \
  struct name : hpx::actions::make_action<                                                                                              \
  decltype(&skeletons::BnB::Dist::searchChildTask<space COMMA sol COMMA bnd COMMA cands COMMA genf COMMA bndf COMMA name>),             \
  &skeletons::BnB::Dist::searchChildTask<space COMMA sol COMMA bnd COMMA cands COMMA genf COMMA bndf COMMA name>,                       \
  name>::type {};                                                                                                                       \
  HPX_REGISTER_ACTION_DECLARATION(name, name);                                                                                          \
  HPX_REGISTER_ACTION(name, name);                                                                                                      \

#define YEWPAR_CREATE_BNB_DIST_ACTION_8(name, space, sol, bnd, cands, genf, bndf, prune)                                                \
  struct name : hpx::actions::make_action<                                                                                              \
  decltype(&skeletons::BnB::Dist::searchChildTask<space COMMA sol COMMA bnd COMMA cands COMMA genf COMMA bndf COMMA name COMMA prune>), \
  &skeletons::BnB::Dist::searchChildTask<space COMMA sol COMMA bnd COMMA cands COMMA genf COMMA bndf COMMA name COMMA prune>,           \
  name>::type {};                                                                                                                       \
  HPX_REGISTER_ACTION_DECLARATION(name, name);                                                                                          \
  HPX_REGISTER_ACTION(name, name);                                                                                                      \

#endif
