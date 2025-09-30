set(YEWPAR_BUILD_BNB_APPS_VERTEXCOVER "ON" CACHE BOOL "Build Branch and Bound Vertex Cover")
set(YEWPAR_BUILD_BNB_APPS_VERTEXCOVER_NWORDS 8 CACHE INT "Number of Words in Branch and Bound Vertex Cover BitSets")

if(YEWPAR_BUILD_BNB_APPS_VERTEXCOVER)
add_hpx_executable(vertexcover-${YEWPAR_BUILD_BNB_APPS_VERTEXCOVER_NWORDS}
  SOURCES main.cpp ../maxclique/DimacsParser.cpp
  COMPILE_FLAGS "-DNWORDS=${YEWPAR_BUILD_BNB_APPS_VERTEXCOVER_NWORDS}"
  DEPENDENCIES YewPar)

if (YEWPAR_BUILD_TEST_APPS)
  add_test(
    NAME VERTEXCOVER_SEQ_1T
    COMMAND vertexcover-${YEWPAR_BUILD_BNB_APPS_VERTEXCOVER_NWORDS} --skeleton seq --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 1)
  set_tests_properties(VERTEXCOVER_SEQ_1T PROPERTIES PASS_REGULAR_EXPRESSION "Vertex Cover Size = 21")

  add_test(
    NAME VERTEXCOVER_ORDERED_1T
    COMMAND vertexcover-${YEWPAR_BUILD_BNB_APPS_VERTEXCOVER_NWORDS} --skeleton ordered -d 1 --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 1)
  set_tests_properties(VERTEXCOVER_ORDERED_1T PROPERTIES PASS_REGULAR_EXPRESSION "Vertex Cover Size = 21")

  add_test(
    NAME VERTEXCOVER_ORDERED_4T
    COMMAND vertexcover-${YEWPAR_BUILD_BNB_APPS_VERTEXCOVER_NWORDS} --skeleton ordered -d 1 --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 4)
  set_tests_properties(VERTEXCOVER_ORDERED_4T PROPERTIES PASS_REGULAR_EXPRESSION "Vertex Cover Size = 21")

  add_test(
    NAME VERTEXCOVER_DEPTHBOUNDED_1T
    COMMAND vertexcover-${YEWPAR_BUILD_BNB_APPS_VERTEXCOVER_NWORDS} -d 1 --skeleton depthbounded --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 1)
  set_tests_properties(VERTEXCOVER_DEPTHBOUNDED_1T PROPERTIES PASS_REGULAR_EXPRESSION "Vertex Cover = 21")

  add_test(
    NAME VERTEXCOVER_DEPTHBOUNDED_4T
    COMMAND vertexcover-${YEWPAR_BUILD_BNB_APPS_VERTEXCOVER_NWORDS} -d 1 --skeleton depthbounded --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 4)
  set_tests_properties(VERTEXCOVER_DEPTHBOUNDED_4T PROPERTIES PASS_REGULAR_EXPRESSION "Vertex Cover = 21")

  add_test(
    NAME VERTEXCOVER_DEPTHBOUNDED_DECISION_1T
    COMMAND vertexcover-${YEWPAR_BUILD_BNB_APPS_VERTEXCOVER_NWORDS} -d 1 --skeleton depthbounded --decisionBound 21 --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 1)
  set_tests_properties(VERTEXCOVER_DEPTHBOUNDED_DECISION_1T PROPERTIES PASS_REGULAR_EXPRESSION "Vertex Cover = 21")

  add_test(
    NAME VERTEXCOVER_DEPTHBOUNDED_DECISION_4T
    COMMAND vertexcover-${YEWPAR_BUILD_BNB_APPS_VERTEXCOVER_NWORDS} -d 1 --skeleton depthbounded --decisionBound 21 --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 4)
  set_tests_properties(VERTEXCOVER_DEPTHBOUNDED_DECISION_4T PROPERTIES PASS_REGULAR_EXPRESSION "Vertex Cover = 21")

  add_test(
    NAME VERTEXCOVER_STACKSTEALS_1T
    COMMAND vertexcover-${YEWPAR_BUILD_BNB_APPS_VERTEXCOVER_NWORDS} --skeleton stacksteal --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 1)
  set_tests_properties(VERTEXCOVER_STACKSTEALS_1T PROPERTIES PASS_REGULAR_EXPRESSION "Vertex Cover = 21")

  add_test(
    NAME VERTEXCOVER_STACKSTEALS_4T
    COMMAND vertexcover-${YEWPAR_BUILD_BNB_APPS_VERTEXCOVER_NWORDS} --skeleton stacksteal --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 4)
  set_tests_properties(VERTEXCOVER_STACKSTEALS_4T PROPERTIES PASS_REGULAR_EXPRESSION "Vertex Cover = 21")

  add_test(
    NAME VERTEXCOVER_BUDGET_1T
    COMMAND vertexcover-${YEWPAR_BUILD_BNB_APPS_VERTEXCOVER_NWORDS} --skeleton budget -b 1000000 --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 1)
  set_tests_properties(VERTEXCOVER_BUDGET_1T PROPERTIES PASS_REGULAR_EXPRESSION "Vertex Cover = 21")

  add_test(
    NAME VERTEXCOVER_BUDGET_4T
    COMMAND vertexcover-${YEWPAR_BUILD_BNB_APPS_VERTEXCOVER_NWORDS} --skeleton budget -b 1000000 --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 4)
  set_tests_properties(VERTEXCOVER_BUDGET_4T PROPERTIES PASS_REGULAR_EXPRESSION "Vertex Cover = 21")
endif (YEWPAR_BUILD_TEST_APPS)

endif(YEWPAR_BUILD_BNB_APPS_VERTEXCOVER)
