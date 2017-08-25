#ifndef YEWPAR_ENUM_MACROS_HPP
#define YEWPAR_ENUM_MACROS_HPP

#define COMMA ,

#define YEWPAR_CREATE_ENUM_ACTION(name, space, sol, genf)                               \
  struct name;                                                                          \
  HPX_ACTION_USES_LARGE_STACK(name);                                                    \
                                                                                        \
  struct name : hpx::actions::make_action<                                              \
    decltype(&skeletons::Enum::searchChildTask<space COMMA sol COMMA genf COMMA name>), \
    &skeletons::Enum::searchChildTask<space COMMA sol COMMA genf COMMA name>,           \
  name>::type {};                                                                       \
  HPX_REGISTER_ACTION_DECLARATION(name, name)                                           \
  HPX_REGISTER_ACTION(name, name)                                                       \

  #define YEWPAR_CREATE_ENUM_INDEXED_ACTION(name, space, sol, genf)                     \
  struct name;                                                                          \
  HPX_ACTION_USES_LARGE_STACK(name);                                                    \
                                                                                        \
  struct name : hpx::actions::make_action<                                              \
    decltype(&skeletons::Enum::Indexed::searchChildTask<space COMMA sol COMMA genf>),   \
    &skeletons::Enum::Indexed::searchChildTask<space COMMA sol COMMA genf>,             \
    name>::type {};                                                                     \
  HPX_REGISTER_ACTION_DECLARATION(name, name)                                           \
  HPX_REGISTER_ACTION(name, name)                                                       \

#endif
