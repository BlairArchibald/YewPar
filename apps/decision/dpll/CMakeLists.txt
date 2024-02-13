set(YEWPAR_BUILD_APPS_DPLL "ON" CACHE BOOL "Build DPLL")

if(YEWPAR_BUILD_APPS_DPLL)

add_hpx_executable(dpll
  SOURCES main.cpp dpll.hpp utils.cpp
  DEPENDENCIES YewPar
)
endif(YEWPAR_BUILD_APPS_DPLL)