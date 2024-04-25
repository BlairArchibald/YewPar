set(YEWPAR_BUILD_ENUMERATION_APPS_MAXIMAL_CLIQUE "ON" CACHE BOOL "Build Maximal Clique Enumeration")

if (YEWPAR_BUILD_ENUMERATION_APPS_MAXIMAL_CLIQUE)
add_hpx_executable(maximalclique
  SOURCES main.cpp DimacsParser.hpp DimacsParser.cpp
  DEPENDENCIES YewPar)

endif (YEWPAR_BUILD_ENUMERATION_APPS_MAXIMAL_CLIQUE)