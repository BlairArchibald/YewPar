set(YEWPAR_BUILD_BNB_APPS_KNAPSACK "ON" CACHE BOOL "Build Branch and Bound Knapsack")
set(YEWPAR_BUILD_BNB_APPS_KNAPSACK_NITEMS 50 CACHE INT "Max number of items in Knapsack item arrays")

if (YEWPAR_BUILD_BNB_APPS_KNAPSACK)
add_hpx_executable(knapsack
  SOURCES main.cpp
  COMPILE_FLAGS "-DNUMITEMS=${YEWPAR_BUILD_BNB_APPS_KNAPSACK_NITEMS}"
  DEPENDENCIES YewPar)

if (YEWPAR_BUILD_TEST_APPS)
  add_test(
    NAME KNAPSACK_SEQ_1T
    COMMAND knapsack --skeleton seq --input-file ${YEWPAR_TEST_DATA_DIR}/knapsackTest1.kp --hpx:threads 1)
  set_tests_properties(KNAPSACK_SEQ_1T PROPERTIES PASS_REGULAR_EXPRESSION "Final Profit: 6925")

  add_test(
    NAME KNAPSACK_DEPTHBOUNDED_1T
    COMMAND knapsack -d 1 --skeleton depthbounded --input-file ${YEWPAR_TEST_DATA_DIR}/knapsackTest1.kp --hpx:threads 1)
  set_tests_properties(KNAPSACK_DEPTHBOUNDED_1T PROPERTIES PASS_REGULAR_EXPRESSION "Final Profit: 6925")

  add_test(
    NAME KNAPSACK_DEPTHBOUNDED_4T
    COMMAND knapsack -d 1 --skeleton depthbounded --input-file ${YEWPAR_TEST_DATA_DIR}/knapsackTest1.kp --hpx:threads 4)
  set_tests_properties(KNAPSACK_DEPTHBOUNDED_4T PROPERTIES PASS_REGULAR_EXPRESSION "Final Profit: 6925")

  add_test(
    NAME KNAPSACK_ORDERED_1T
    COMMAND knapsack -d 1 --skeleton ordered --input-file ${YEWPAR_TEST_DATA_DIR}/knapsackTest1.kp --hpx:threads 1)
  set_tests_properties(KNAPSACK_ORDERED_1T PROPERTIES PASS_REGULAR_EXPRESSION "Final Profit: 6925")

  add_test(
    NAME KNAPSACK_ORDERED_4T
    COMMAND knapsack -d 1 --skeleton ordered --input-file ${YEWPAR_TEST_DATA_DIR}/knapsackTest1.kp --hpx:threads 4)
  set_tests_properties(KNAPSACK_ORDERED_4T PROPERTIES PASS_REGULAR_EXPRESSION "Final Profit: 6925")
endif (YEWPAR_BUILD_TEST_APPS)

endif (YEWPAR_BUILD_BNB_APPS_KNAPSACK)
