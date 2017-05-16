add_hpx_executable(maxclique
  SOURCES main.cpp DimacsParser.cpp
  COMPONENT_DEPENDENCIES incumbent workqueue
  DEPENDENCIES workstealing_lib)
