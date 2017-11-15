add_subdirectory(bnb)
add_subdirectory(enumerate)
add_subdirectory(workstealing)
add_subdirectory(util)

add_hpx_library(YewPar
  SOURCES
  workstealing/Scheduler.cpp

  COMPONENT_DEPENDENCIES
  workqueue
  priorityworkqueue
  incumbent
  SearchManager
  iostreams
)
