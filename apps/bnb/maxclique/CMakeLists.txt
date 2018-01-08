set(YEWPAR_BUILD_BNB_APPS_MAXCLIQUE "ON" CACHE BOOL "Build Branch and Bound Maximum Clique")
set(YEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS 8 CACHE INT "Number of Words in Branch and Bound Maximum Clique BitSets")

if(YEWPAR_BUILD_BNB_APPS_MAXCLIQUE)
add_hpx_executable(maxclique-${YEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS}
  SOURCES main.cpp DimacsParser.cpp
  COMPILE_FLAGS "-DNWORDS=${YEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS}"
  DEPENDENCIES YewPar_lib)


if (YEWPAR_BUILD_TEST_APPS)
  # TODO: Can make this much neater with a loop
  add_test(
    NAME MAXCLIQUE_SEQ_1T
    COMMAND maxclique-${YEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS} --skeleton-type seq --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 1)
  set_tests_properties(MAXCLIQUE_SEQ_1T PROPERTIES PASS_REGULAR_EXPRESSION "MaxClique Size = 21")

  add_test(
    NAME MAXCLIQUE_PAR_1T
    COMMAND maxclique-${YEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS} --skeleton-type par --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 1)
  set_tests_properties(MAXCLIQUE_PAR_1T PROPERTIES PASS_REGULAR_EXPRESSION "MaxClique Size = 21")

  add_test(
    NAME MAXCLIQUE_PAR_4T
    COMMAND maxclique-${YEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS} --skeleton-type par --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 4)
  set_tests_properties(MAXCLIQUE_PAR_4T PROPERTIES PASS_REGULAR_EXPRESSION "MaxClique Size = 21")

  add_test(
    NAME MAXCLIQUE_ORDERED_1T
    COMMAND maxclique-${YEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS} --skeleton-type ordered --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 1)
  set_tests_properties(MAXCLIQUE_ORDERED_1T PROPERTIES PASS_REGULAR_EXPRESSION "MaxClique Size = 21")

  add_test(
    NAME MAXCLIQUE_ORDERED_4T
    COMMAND maxclique-${YEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS} --skeleton-type ordered --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 4)
  set_tests_properties(MAXCLIQUE_ORDERED_4T PROPERTIES PASS_REGULAR_EXPRESSION "MaxClique Size = 21")

  add_test(
    NAME MAXCLIQUE_DIST_1T
    COMMAND maxclique-${YEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS} -d 1 --skeleton-type dist --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 1)
  set_tests_properties(MAXCLIQUE_DIST_1T PROPERTIES PASS_REGULAR_EXPRESSION "MaxClique Size = 21")

  add_test(
    NAME MAXCLIQUE_DIST_4T
    COMMAND maxclique-${YEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS} -d 1 --skeleton-type dist --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 4)
  set_tests_properties(MAXCLIQUE_DIST_4T PROPERTIES PASS_REGULAR_EXPRESSION "MaxClique Size = 21")

  add_test(
    NAME MAXCLIQUE_DIST_DECISION_1T
    COMMAND maxclique-${YEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS} -d 1 --skeleton-type dist-decision --decisionBound 21 --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 1)
  set_tests_properties(MAXCLIQUE_DIST_DECISION_1T PROPERTIES PASS_REGULAR_EXPRESSION "MaxClique Size = 21")

  add_test(
    NAME MAXCLIQUE_DIST_DECISION_4T
    COMMAND maxclique-${YEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS} -d 1 --skeleton-type dist-decision --decisionBound 21 --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 4)
  set_tests_properties(MAXCLIQUE_DIST_DECISION_4T PROPERTIES PASS_REGULAR_EXPRESSION "MaxClique Size = 21")

  add_test(
    NAME MAXCLIQUE_DISTRECOMPUTE_1T
    COMMAND maxclique-${YEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS} -d 1 --skeleton-type dist-recompute --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 1)
  set_tests_properties(MAXCLIQUE_DISTRECOMPUTE_1T PROPERTIES PASS_REGULAR_EXPRESSION "MaxClique Size = 21")

  add_test(
    NAME MAXCLIQUE_DISTRECOMPUTE_4T
    COMMAND maxclique-${YEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS} -d 1 --skeleton-type dist-recompute --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 4)
  set_tests_properties(MAXCLIQUE_DISTRECOMPUTE_4T PROPERTIES PASS_REGULAR_EXPRESSION "MaxClique Size = 21")

  add_test(
    NAME MAXCLIQUE_INDEXED_1T
    COMMAND maxclique-${YEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS} --skeleton-type indexed --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 1)
  set_tests_properties(MAXCLIQUE_INDEXED_1T PROPERTIES PASS_REGULAR_EXPRESSION "MaxClique Size = 21")

  add_test(
    NAME MAXCLIQUE_INDEXED_4T
    COMMAND maxclique-${YEWPAR_BUILD_BNB_APPS_MAXCLIQUE_NWORDS} --skeleton-type indexed --input-file ${YEWPAR_TEST_DATA_DIR}/brock200_1.clq --hpx:threads 4)
  set_tests_properties(MAXCLIQUE_INDEXED_4T PROPERTIES PASS_REGULAR_EXPRESSION "MaxClique Size = 21")
endif (YEWPAR_BUILD_TEST_APPS)

endif(YEWPAR_BUILD_BNB_APPS_MAXCLIQUE)
