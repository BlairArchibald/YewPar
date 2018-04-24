set(YEWPAR_BUILD_APPS_SIP "ON" CACHE BOOL "Build Subgraph Isomorphism")
set(YEWPAR_BUILD_APPS_SIP_NWORDS 128 CACHE INT "Number of Words in SIP Domain BitSets")

if(YEWPAR_BUILD_APPS_SIP)
add_hpx_executable(sip
  SOURCES main.cpp lad.cc graph.cc graph_file_error.cc fixed_bit_set.cc
  COMPILE_FLAGS "-DNWORDS=${YEWPAR_BUILD_APPS_SIP_NWORDS}"
  DEPENDENCIES YewPar_lib)
endif(YEWPAR_BUILD_APPS_SIP)
