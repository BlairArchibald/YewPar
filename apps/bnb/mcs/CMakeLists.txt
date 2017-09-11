set(YEWPAR_BUILD_BNB_APPS_MCS "ON" CACHE BOOL "Build Branch and Bound Maximum Common Subgraph")
set(YEWPAR_BUILD_BNB_APPS_MCS_NWORDS 8 CACHE INT "Number of Words in Branch and Bound Maximum Common Subgraph")

if (YEWPAR_BUILD_BNB_APPS_MCS)
add_hpx_executable(mcs-${YEWPAR_BUILD_BNB_APPS_MCS_NWORDS}
  SOURCES main.cpp VFParser.cpp
  COMPILE_FLAGS "-DNWORDS=${YEWPAR_BUILD_BNB_APPS_MCS_NWORDS}"
  DEPENDENCIES YewPar_lib)
endif (YEWPAR_BUILD_BNB_APPS_MCS)
