add_hpx_executable(knapsack
  SOURCES main.cpp
  COMPONENT_DEPENDENCIES incumbent workqueue
  DEPENDENCIES workstealing_lib)
