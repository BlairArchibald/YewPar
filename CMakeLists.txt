# Require a recent version of cmake
cmake_minimum_required(VERSION 3.5.1 FATAL_ERROR)

# This project is C++ based.
project(YewPar CXX)

# Instruct cmake to find the HPX settings
find_package(HPX)

include_directories(lib)

add_subdirectory(lib)
add_subdirectory(apps)
