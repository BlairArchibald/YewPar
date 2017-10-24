add_subdirectory(bnb)
add_subdirectory(enumerate)
add_subdirectory(workstealing)
add_subdirectory(util)

add_hpx_library(YewPar
  SOURCES
  workstealing/scheduler.cpp
  workstealing/priorityscheduler.cpp

  COMPONENT_DEPENDENCIES
  workqueue
  priorityworkqueue
  incumbent
  SearchManager
  iostreams
)
