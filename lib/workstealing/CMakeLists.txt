add_hpx_component(workqueue
    SOURCES workqueue.cpp
    HEADERS workqueue.hpp)

add_hpx_component(priorityworkqueue
    SOURCES priorityworkqueue.cpp
    HEADERS priorityworkqueue.hpp)

add_hpx_library(workstealing
  SOURCES scheduler.cpp priorityscheduler.cpp
)
