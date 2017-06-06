add_hpx_executable(fib
  SOURCES main.cpp
  COMPONENT_DEPENDENCIES workqueue priorityworkqueue
  DEPENDENCIES workstealing_lib)
