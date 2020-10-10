set(YEWPAR_BUILD_ENUMERATION_APPS_UTS "ON" CACHE BOOL "Build UTS")

if(YEWPAR_BUILD_ENUMERATION_APPS_UTS)
include_directories(uts-rng)

add_hpx_executable(uts
  SOURCES main.cpp uts-rng/brg_sha1.c
  DEPENDENCIES YewPar)
endif(YEWPAR_BUILD_ENUMERATION_APPS_UTS)

if (YEWPAR_BUILD_TEST_APPS)
  add_test(UTS_SEQ_1T uts --skeleton seq --uts-t geometric --uts-a 2 --uts-d 10 --uts-b 4 --uts-r 19 --hpx:threads 1)
  set_tests_properties(UTS_SEQ_1T PROPERTIES PASS_REGULAR_EXPRESSION "Total Nodes: 4130071")

  add_test(UTS_DEPTHBOUNDED_1T uts -s 3 --skeleton depthbounded --uts-t geometric --uts-a 2 --uts-d 10 --uts-b 4 --uts-r 19 --hpx:threads 1)
  set_tests_properties(UTS_DEPTHBOUNDED_1T PROPERTIES PASS_REGULAR_EXPRESSION "Total Nodes: 4130071")

  add_test(UTS_DEPTHBOUNDED_4T uts -s 3 --skeleton depthbounded --uts-t geometric --uts-a 2 --uts-d 10 --uts-b 4 --uts-r 19 --hpx:threads 4)
  set_tests_properties(UTS_DEPTHBOUNDED_4T PROPERTIES PASS_REGULAR_EXPRESSION "Total Nodes: 4130071")

  add_test(UTS_STACKSTEAL_1T uts --skeleton stacksteal --uts-t geometric --uts-a 2 --uts-d 10 --uts-b 4 --uts-r 19 --hpx:threads 1)
  set_tests_properties(UTS_STACKSTEAL_1T PROPERTIES PASS_REGULAR_EXPRESSION "Total Nodes: 4130071")

  add_test(UTS_STACKSTEAL_4T uts --skeleton stacksteal --uts-t geometric --uts-a 2 --uts-d 10 --uts-b 4 --uts-r 19 --hpx:threads 4)
  set_tests_properties(UTS_STACKSTEAL_4T PROPERTIES PASS_REGULAR_EXPRESSION "Total Nodes: 4130071")
endif (YEWPAR_BUILD_TEST_APPS)
