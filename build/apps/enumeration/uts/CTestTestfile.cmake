# CMake generated Testfile for 
# Source directory: /home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/apps/enumeration/uts
# Build directory: /home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/build/apps/enumeration/uts
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(UTS_SEQ_1T "uts" "--skeleton" "seq" "--uts-t" "geometric" "--uts-a" "2" "--uts-d" "10" "--uts-b" "4" "--uts-r" "19" "--hpx:threads" "1")
set_tests_properties(UTS_SEQ_1T PROPERTIES  PASS_REGULAR_EXPRESSION "Total Nodes: 4130071" _BACKTRACE_TRIPLES "/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/apps/enumeration/uts/CMakeLists.txt;12;add_test;/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/apps/enumeration/uts/CMakeLists.txt;0;")
add_test(UTS_DEPTHBOUNDED_1T "uts" "-s" "3" "--skeleton" "depthbounded" "--uts-t" "geometric" "--uts-a" "2" "--uts-d" "10" "--uts-b" "4" "--uts-r" "19" "--hpx:threads" "1")
set_tests_properties(UTS_DEPTHBOUNDED_1T PROPERTIES  PASS_REGULAR_EXPRESSION "Total Nodes: 4130071" _BACKTRACE_TRIPLES "/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/apps/enumeration/uts/CMakeLists.txt;15;add_test;/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/apps/enumeration/uts/CMakeLists.txt;0;")
add_test(UTS_DEPTHBOUNDED_4T "uts" "-s" "3" "--skeleton" "depthbounded" "--uts-t" "geometric" "--uts-a" "2" "--uts-d" "10" "--uts-b" "4" "--uts-r" "19" "--hpx:threads" "4")
set_tests_properties(UTS_DEPTHBOUNDED_4T PROPERTIES  PASS_REGULAR_EXPRESSION "Total Nodes: 4130071" _BACKTRACE_TRIPLES "/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/apps/enumeration/uts/CMakeLists.txt;18;add_test;/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/apps/enumeration/uts/CMakeLists.txt;0;")
add_test(UTS_STACKSTEAL_1T "uts" "--skeleton" "stacksteal" "--uts-t" "geometric" "--uts-a" "2" "--uts-d" "10" "--uts-b" "4" "--uts-r" "19" "--hpx:threads" "1")
set_tests_properties(UTS_STACKSTEAL_1T PROPERTIES  PASS_REGULAR_EXPRESSION "Total Nodes: 4130071" _BACKTRACE_TRIPLES "/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/apps/enumeration/uts/CMakeLists.txt;21;add_test;/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/apps/enumeration/uts/CMakeLists.txt;0;")
add_test(UTS_STACKSTEAL_4T "uts" "--skeleton" "stacksteal" "--uts-t" "geometric" "--uts-a" "2" "--uts-d" "10" "--uts-b" "4" "--uts-r" "19" "--hpx:threads" "4")
set_tests_properties(UTS_STACKSTEAL_4T PROPERTIES  PASS_REGULAR_EXPRESSION "Total Nodes: 4130071" _BACKTRACE_TRIPLES "/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/apps/enumeration/uts/CMakeLists.txt;24;add_test;/home/ruairidh/Documents/University/Evaluating-Parallel-Search-On-HPC-Cloud/YewPar/apps/enumeration/uts/CMakeLists.txt;0;")
