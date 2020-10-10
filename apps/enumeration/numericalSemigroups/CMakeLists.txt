set(YEWPAR_BUILD_ENUMERATION_APPS_NS_BASIC "OFF" CACHE BOOL "Build Basic Numerical Semigroups Enumeration")
set(YEWPAR_BUILD_ENUMERATION_APPS_NS_HIVERT "ON" CACHE BOOL "Build Hivert's Numerical Semigroups Enumeration")
set(YEWPAR_BUILD_ENUMERATION_APPS_NS_HIVERT_MAXGENUS 50 CACHE INT "Max Depth/Genus for Hivert's Numerical Semigroups Enumeration")

if (YEWPAR_BUILD_ENUMERATION_APPS_NS_BASIC)
add_hpx_executable(NS-basic
  SOURCES basic.cpp
  DEPENDENCIES YewPar)

if (YEWPAR_BUILD_TEST_APPS)
  add_test(NS_BASIC_DIST_1T NS-basic -d 30 -s 10 --hpx:threads 1)
  set_tests_properties(NS_BASIC_DIST_1T PROPERTIES PASS_REGULAR_EXPRESSION "30: 5646773")
  add_test(NS_BASIC_DIST_4T NS-basic -d 30 -s 10 --hpx:threads 4)
  set_tests_properties(NS_BASIC_DIST_4T PROPERTIES PASS_REGULAR_EXPRESSION "30: 5646773")
endif (YEWPAR_BUILD_TEST_APPS)

endif (YEWPAR_BUILD_ENUMERATION_APPS_NS_BASIC)

if (YEWPAR_BUILD_ENUMERATION_APPS_NS_HIVERT)
add_hpx_executable(NS-hivert
  SOURCES hivert.cpp monoid.cpp
  COMPILE_FLAGS "-DMAX_GENUS=${YEWPAR_BUILD_ENUMERATION_APPS_NS_HIVERT_MAXGENUS} -mssse3 -mpopcnt"
  DEPENDENCIES YewPar)

if (YEWPAR_BUILD_TEST_APPS)
  add_test(NS_HIVERT_SEQ_1T NS-hivert --skeleton seq -d 30 --hpx:threads 1)
  set_tests_properties(NS_HIVERT_SEQ_1T PROPERTIES PASS_REGULAR_EXPRESSION "30: 5646773")

  add_test(NS_HIVERT_DEPTHBOUNDED_1T NS-hivert --skeleton depthbounded -d 30 -s 10 --hpx:threads 1)
  set_tests_properties(NS_HIVERT_DEPTHBOUNDED_1T PROPERTIES PASS_REGULAR_EXPRESSION "30: 5646773")

  add_test(NS_HIVERT_DEPTHBOUNDED_4T NS-hivert --skeleton depthbounded -d 30 -s 10 --hpx:threads 4)
  set_tests_properties(NS_HIVERT_DEPTHBOUNDED_4T PROPERTIES PASS_REGULAR_EXPRESSION "30: 5646773")

  add_test(NS_HIVERT_STACKSTEALS_1T NS-hivert --skeleton stacksteal -d 30 --hpx:threads 1)
  set_tests_properties(NS_HIVERT_STACKSTEALS_1T PROPERTIES PASS_REGULAR_EXPRESSION "30: 5646773")

  add_test(NS_HIVERT_STACKSTEALS_4T NS-hivert --skeleton stacksteal -d 30 --hpx:threads 4)
  set_tests_properties(NS_HIVERT_STACKSTEALS_4T PROPERTIES PASS_REGULAR_EXPRESSION "30: 5646773")
endif (YEWPAR_BUILD_TEST_APPS)

endif (YEWPAR_BUILD_ENUMERATION_APPS_NS_HIVERT)
