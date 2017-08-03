add_hpx_executable(NS-basic
  SOURCES basic.cpp
  DEPENDENCIES YewPar_lib)

add_hpx_executable(NS-hivert
  SOURCES hivert.cpp monoid.cpp
  COMPILE_FLAGS "-DMAX_GENUS=70"
  DEPENDENCIES YewPar_lib)
