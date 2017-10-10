set(YEWPAR_BUILD_ENUMERATION_APPS_FIB "ON" CACHE BOOL "Build Fib Enumeration - for testing mainly")

if(YEWPAR_BUILD_ENUMERATION_APPS_FIB)
add_hpx_executable(enum-fib
  SOURCES main.cpp
  DEPENDENCIES YewPar_lib)
endif(YEWPAR_BUILD_ENUMERATION_APPS_FIB)
