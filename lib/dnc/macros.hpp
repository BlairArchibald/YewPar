#ifndef YEWPAR_MACROS_DNC_HPP
#define YEWPAR_MACROS_DNC_HPP

#define COMMA ,

#define YEWPAR_CREATE_DNC_PAR_ACTION(name, ret, div, conq, triv, fn)                              \
  struct name : hpx::actions::make_action<                                                        \
    decltype(&skeletons::DnC::Par::dnc<ret COMMA div COMMA conq COMMA triv COMMA fn COMMA name>), \
    &skeletons::DnC::Par::dnc<ret COMMA div COMMA conq COMMA triv COMMA fn COMMA name>,           \
    name>::type {};                                                                               \
  HPX_REGISTER_ACTION_DECLARATION(name, name);                                                    \
  HPX_REGISTER_ACTION(name, name);                                                                \


#define YEWPAR_CREATE_DNC_DIST_ACTION(name, ret, div, conq, triv, fn)                                \
  struct name : hpx::actions::make_action<                                                           \
  decltype(&skeletons::DnC::Dist::dncTask<ret COMMA div COMMA conq COMMA triv COMMA fn COMMA name>), \
  &skeletons::DnC::Dist::dncTask<ret COMMA div COMMA conq COMMA triv COMMA fn COMMA name>,           \
    name>::type {};                                                                                  \
  HPX_REGISTER_ACTION_DECLARATION(name, name);                                                       \
  HPX_REGISTER_ACTION(name, name);                                                                   \

#endif
