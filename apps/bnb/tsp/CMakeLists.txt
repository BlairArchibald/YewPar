set(YEWPAR_BUILD_BNB_APPS_TSP "ON" CACHE BOOL "Build Branch and Bound Travelling Salesman")

if (YEWPAR_BUILD_BNB_APPS_TSP)
  add_hpx_executable(tsp
    SOURCES main.cpp parser.cpp
    DEPENDENCIES YewPar_lib)
endif (YEWPAR_BUILD_BNB_APPS_TSP)

if (YEWPAR_BUILD_TEST_APPS)
  add_test(
    NAME TSP_SEQ_1T
    COMMAND tsp  --skeleton seq --input-file ${YEWPAR_TEST_DATA_DIR}/burma14.tsp --hpx:threads 1)
  set_tests_properties(TSP_SEQ_1T PROPERTIES PASS_REGULAR_EXPRESSION "Optimal tour length: 3323")

  add_test(
    NAME TSP_DEPTHBOUNDED_1T
    COMMAND tsp -d 1 --skeleton depthbounded --input-file ${YEWPAR_TEST_DATA_DIR}/burma14.tsp --hpx:threads 1)
  set_tests_properties(TSP_DEPTHBOUNDED_1T PROPERTIES PASS_REGULAR_EXPRESSION "Optimal tour length: 3323")

  add_test(
    NAME TSP_DEPTHBOUNDED_4T
    COMMAND tsp -d 1 --skeleton depthbounded --input-file ${YEWPAR_TEST_DATA_DIR}/burma14.tsp --hpx:threads 4)
  set_tests_properties(TSP_DEPTHBOUNDED_4T PROPERTIES PASS_REGULAR_EXPRESSION "Optimal tour length: 3323")

  add_test(
    NAME TSP_ORDERED_1T
    COMMAND tsp -d 1 --skeleton ordered --input-file ${YEWPAR_TEST_DATA_DIR}/burma14.tsp --hpx:threads 1)
  set_tests_properties(TSP_ORDERED_1T PROPERTIES PASS_REGULAR_EXPRESSION "Optimal tour length: 3323")

  add_test(
    NAME TSP_ORDERED_4T
    COMMAND tsp -d 1 --skeleton ordered --input-file ${YEWPAR_TEST_DATA_DIR}/burma14.tsp --hpx:threads 4)
  set_tests_properties(TSP_ORDERED_4T PROPERTIES PASS_REGULAR_EXPRESSION "Optimal tour length: 3323")

endif (YEWPAR_BUILD_TEST_APPS)
