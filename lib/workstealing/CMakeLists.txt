add_subdirectory(policies)

add_hpx_component(Workqueue
    SOURCES Workqueue.cpp
    HEADERS Workqueue.hpp)

add_hpx_component(DepthPool
    SOURCES DepthPool.cpp
    HEADERS DepthPool.hpp)

add_hpx_component(PriorityWorkqueue
    SOURCES PriorityWorkqueue.cpp
    HEADERS PriorityWorkqueue.hpp)
