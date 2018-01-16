set(YEWPAR_BUILD_BNB_APPS_TSP "ON" CACHE BOOL "Build Branch and Bound Travelling Salesman")

if (YEWPAR_BUILD_BNB_APPS_TSP)
  add_hpx_executable(tsp
    SOURCES main.cpp parser.cpp
    DEPENDENCIES YewPar_lib)
endif (YEWPAR_BUILD_BNB_APPS_TSP)
