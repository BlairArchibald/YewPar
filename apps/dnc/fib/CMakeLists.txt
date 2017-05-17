add_hpx_executable(fib
  SOURCES main.cpp
  COMPONENT_DEPENDENCIES workqueue
  DEPENDENCIES workstealing_lib)
