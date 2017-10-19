set(YEWPAR_BUILD_ENUMERATION_APPS_UTS "ON" CACHE BOOL "Build UTS")

if(YEWPAR_BUILD_ENUMERATION_APPS_UTS)
include_directories(uts-rng)

add_hpx_executable(uts
  SOURCES main.cpp uts-rng/brg_sha1.c
  DEPENDENCIES YewPar_lib)
endif(YEWPAR_BUILD_ENUMERATION_APPS_UTS)
