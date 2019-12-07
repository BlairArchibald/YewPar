# Require a recent version of cmake
cmake_minimum_required(VERSION 3.8.0 FATAL_ERROR)

# This project is C++ based.
project(YewPar CXX)

# Instruct cmake to find the HPX settings
find_package(HPX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# Variables to allow toggling of example apps
set(YEWPAR_LIBRARY_ONLY "OFF" CACHE BOOL "Build YewPar without any applications")
set(YEWPAR_BUILD_DNC_APPS "OFF" CACHE BOOL "Build Divide and Conquer apps for YewPar")
set(YEWPAR_BUILD_BNB_APPS "ON" CACHE BOOL "Build Branch and Bound apps for YewPar")
set(YEWPAR_BUILD_ENUMERATION_APPS "ON" CACHE BOOL "Build Enumeration apps for YewPar")
set(YEWPAR_BUILD_TEST_APPS "ON" CACHE BOOL "Create tests for YewPar apps")

set(YEWPAR_TEST_DATA_DIR "${PROJECT_SOURCE_DIR}/test/" CACHE FILEPATH "Test data directory for YewPar apps")

include_directories(lib)

if (YEWPAR_BUILD_TEST_APPS)
  enable_testing()
endif(YEWPAR_BUILD_TEST_APPS)

add_subdirectory(lib)
if(NOT YEWPAR_LIBRARY_ONLY)
  add_subdirectory(apps)
endif(NOT YEWPAR_LIBRARY_ONLY)
