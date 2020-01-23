# Install script for directory: /home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/apps/enumeration/numericalSemigroups

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/build/install")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "0")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/NS-basic" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/NS-basic")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/NS-basic"
         RPATH "/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/build/install/lib")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/build/apps/enumeration/numericalSemigroups/NS-basic")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/NS-basic" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/NS-basic")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/NS-basic"
         OLD_RPATH "/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/build/lib:/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/build/lib/workstealing:/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/build/lib/util:/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/build/lib/workstealing/policies:/usr/local/lib:"
         NEW_RPATH "/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/build/install/lib")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/NS-basic")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/NS-hivert" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/NS-hivert")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/NS-hivert"
         RPATH "/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/build/install/lib")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/build/apps/enumeration/numericalSemigroups/NS-hivert")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/NS-hivert" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/NS-hivert")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/NS-hivert"
         OLD_RPATH "/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/build/lib:/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/build/lib/workstealing:/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/build/lib/util:/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/build/lib/workstealing/policies:/usr/local/lib:"
         NEW_RPATH "/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/build/install/lib")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/NS-hivert")
    endif()
  endif()
endif()

