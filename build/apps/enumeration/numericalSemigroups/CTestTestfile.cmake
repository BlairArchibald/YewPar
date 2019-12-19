# CMake generated Testfile for 
# Source directory: /users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/enumeration/numericalSemigroups
# Build directory: /users/level4/2250079m/sandbox/YewParInstall/YewPar/build/apps/enumeration/numericalSemigroups
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(NS_BASIC_DIST_1T "NS-basic" "-d" "30" "-s" "10" "--hpx:threads" "1")
set_tests_properties(NS_BASIC_DIST_1T PROPERTIES  PASS_REGULAR_EXPRESSION "30: 5646773" _BACKTRACE_TRIPLES "/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/enumeration/numericalSemigroups/CMakeLists.txt;11;add_test;/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/enumeration/numericalSemigroups/CMakeLists.txt;0;")
add_test(NS_BASIC_DIST_4T "NS-basic" "-d" "30" "-s" "10" "--hpx:threads" "4")
set_tests_properties(NS_BASIC_DIST_4T PROPERTIES  PASS_REGULAR_EXPRESSION "30: 5646773" _BACKTRACE_TRIPLES "/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/enumeration/numericalSemigroups/CMakeLists.txt;13;add_test;/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/enumeration/numericalSemigroups/CMakeLists.txt;0;")
add_test(NS_HIVERT_SEQ_1T "NS-hivert" "--skeleton" "seq" "-d" "30" "--hpx:threads" "1")
set_tests_properties(NS_HIVERT_SEQ_1T PROPERTIES  PASS_REGULAR_EXPRESSION "30: 5646773" _BACKTRACE_TRIPLES "/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/enumeration/numericalSemigroups/CMakeLists.txt;26;add_test;/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/enumeration/numericalSemigroups/CMakeLists.txt;0;")
add_test(NS_HIVERT_DEPTHBOUNDED_1T "NS-hivert" "--skeleton" "depthbounded" "-d" "30" "-s" "10" "--hpx:threads" "1")
set_tests_properties(NS_HIVERT_DEPTHBOUNDED_1T PROPERTIES  PASS_REGULAR_EXPRESSION "30: 5646773" _BACKTRACE_TRIPLES "/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/enumeration/numericalSemigroups/CMakeLists.txt;29;add_test;/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/enumeration/numericalSemigroups/CMakeLists.txt;0;")
add_test(NS_HIVERT_DEPTHBOUNDED_4T "NS-hivert" "--skeleton" "depthbounded" "-d" "30" "-s" "10" "--hpx:threads" "4")
set_tests_properties(NS_HIVERT_DEPTHBOUNDED_4T PROPERTIES  PASS_REGULAR_EXPRESSION "30: 5646773" _BACKTRACE_TRIPLES "/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/enumeration/numericalSemigroups/CMakeLists.txt;32;add_test;/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/enumeration/numericalSemigroups/CMakeLists.txt;0;")
add_test(NS_HIVERT_STACKSTEALS_1T "NS-hivert" "--skeleton" "stacksteal" "-d" "30" "--hpx:threads" "1")
set_tests_properties(NS_HIVERT_STACKSTEALS_1T PROPERTIES  PASS_REGULAR_EXPRESSION "30: 5646773" _BACKTRACE_TRIPLES "/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/enumeration/numericalSemigroups/CMakeLists.txt;35;add_test;/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/enumeration/numericalSemigroups/CMakeLists.txt;0;")
add_test(NS_HIVERT_STACKSTEALS_4T "NS-hivert" "--skeleton" "stacksteal" "-d" "30" "--hpx:threads" "4")
set_tests_properties(NS_HIVERT_STACKSTEALS_4T PROPERTIES  PASS_REGULAR_EXPRESSION "30: 5646773" _BACKTRACE_TRIPLES "/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/enumeration/numericalSemigroups/CMakeLists.txt;38;add_test;/users/level4/2250079m/sandbox/YewParInstall/YewPar/apps/enumeration/numericalSemigroups/CMakeLists.txt;0;")
