add_subdirectory(bnb)
add_subdirectory(workstealing)

add_hpx_library(YewPar
  SOURCES
  workstealing/workstealing.cpp
  workstealing/scheduler.cpp
  workstealing/priorityscheduler.cpp
  workstealing/indexedScheduler.cpp

  COMPONENT_DEPENDENCIES
  workqueue
  priorityworkqueue
  incumbent
  posManager
)
