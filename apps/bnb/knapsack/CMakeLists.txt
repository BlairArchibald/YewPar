add_hpx_executable(knapsack
  SOURCES main.cpp
  COMPONENT_DEPENDENCIES incumbent workqueue priorityworkqueue
  DEPENDENCIES workstealing_lib)
