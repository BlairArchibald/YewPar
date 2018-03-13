set(YEWPAR_BUILD_APPS_SIP "ON" CACHE BOOL "Build Subgraph Isomorphism")

if(YEWPAR_BUILD_APPS_SIP)
add_hpx_executable(sip
  SOURCES main.cpp lad.cc graph.cc graph_file_error.cc fixed_bit_set.cc
  DEPENDENCIES YewPar_lib)
endif(YEWPAR_BUILD_APPS_SIP)
