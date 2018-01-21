add_subdirectory(workstealing)
add_subdirectory(skeletons)
add_subdirectory(util)

add_hpx_library(YewPar
  SOURCES
  workstealing/Scheduler.cpp

  COMPONENT_DEPENDENCIES
  workqueue
  priorityworkqueue
  Incumbent
  SearchManager
)
