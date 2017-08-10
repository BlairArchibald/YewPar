# Require a recent version of cmake
cmake_minimum_required(VERSION 3.4.0 FATAL_ERROR)

# This project is C++ based.
project(YewPar CXX)

# Instruct cmake to find the HPX settings
find_package(HPX)

# Variables to allow toggling of example apps
set(YEWPAR_LIBRARY_ONLY "OFF" CACHE BOOL "Build YewPar without any applications")
set(YEWPAR_BUILD_DNC_APPS "ON" CACHE BOOL "Build Divide and Conquer apps for YewPar")
set(YEWPAR_BUILD_BNB_APPS "ON" CACHE BOOL "Build Branch and Bound apps for YewPar")
set(YEWPAR_BUILD_ENUMERATION_APPS "ON" CACHE BOOL "Build Enumeration apps for YewPar")

include_directories(lib)

add_subdirectory(lib)
if(NOT YEWPAR_LIBRARY_ONLY)
  add_subdirectory(apps)
endif(NOT YEWPAR_LIBRARY_ONLY)
