set(YEWPAR_BUILD_BNB_APPS_KNAPSACK "ON" CACHE BOOL "Build Branch and Bound Knapsack")

if (YEWPAR_BUILD_BNB_APPS_KNAPSACK)
add_hpx_executable(knapsack
  SOURCES main.cpp
  DEPENDENCIES YewPar_lib)
endif (YEWPAR_BUILD_BNB_APPS_KNAPSACK)
