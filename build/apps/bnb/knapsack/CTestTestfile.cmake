# CMake generated Testfile for 
# Source directory: /users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/bnb/knapsack
# Build directory: /users/level4/2250079m/sandbox/YewParInstall/YewPar/build/apps/bnb/knapsack
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(KNAPSACK_SEQ_1T "knapsack" "--skeleton" "seq" "--input-file" "/users/level4/2250079m/sandbox/YewParInstall/YewPar/test/knapsackTest1.kp" "--hpx:threads" "1")
set_tests_properties(KNAPSACK_SEQ_1T PROPERTIES  PASS_REGULAR_EXPRESSION "Final Profit: 6925" _BACKTRACE_TRIPLES "/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/bnb/knapsack/CMakeLists.txt;11;add_test;/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/bnb/knapsack/CMakeLists.txt;0;")
add_test(KNAPSACK_DEPTHBOUNDED_1T "knapsack" "-d" "1" "--skeleton" "depthbounded" "--input-file" "/users/level4/2250079m/sandbox/YewParInstall/YewPar/test/knapsackTest1.kp" "--hpx:threads" "1")
set_tests_properties(KNAPSACK_DEPTHBOUNDED_1T PROPERTIES  PASS_REGULAR_EXPRESSION "Final Profit: 6925" _BACKTRACE_TRIPLES "/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/bnb/knapsack/CMakeLists.txt;16;add_test;/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/bnb/knapsack/CMakeLists.txt;0;")
add_test(KNAPSACK_DEPTHBOUNDED_4T "knapsack" "-d" "1" "--skeleton" "depthbounded" "--input-file" "/users/level4/2250079m/sandbox/YewParInstall/YewPar/test/knapsackTest1.kp" "--hpx:threads" "4")
set_tests_properties(KNAPSACK_DEPTHBOUNDED_4T PROPERTIES  PASS_REGULAR_EXPRESSION "Final Profit: 6925" _BACKTRACE_TRIPLES "/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/bnb/knapsack/CMakeLists.txt;21;add_test;/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/bnb/knapsack/CMakeLists.txt;0;")
add_test(KNAPSACK_ORDERED_1T "knapsack" "-d" "1" "--skeleton" "ordered" "--input-file" "/users/level4/2250079m/sandbox/YewParInstall/YewPar/test/knapsackTest1.kp" "--hpx:threads" "1")
set_tests_properties(KNAPSACK_ORDERED_1T PROPERTIES  PASS_REGULAR_EXPRESSION "Final Profit: 6925" _BACKTRACE_TRIPLES "/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/bnb/knapsack/CMakeLists.txt;26;add_test;/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/bnb/knapsack/CMakeLists.txt;0;")
add_test(KNAPSACK_ORDERED_4T "knapsack" "-d" "1" "--skeleton" "ordered" "--input-file" "/users/level4/2250079m/sandbox/YewParInstall/YewPar/test/knapsackTest1.kp" "--hpx:threads" "4")
set_tests_properties(KNAPSACK_ORDERED_4T PROPERTIES  PASS_REGULAR_EXPRESSION "Final Profit: 6925" _BACKTRACE_TRIPLES "/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/bnb/knapsack/CMakeLists.txt;31;add_test;/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/bnb/knapsack/CMakeLists.txt;0;")
