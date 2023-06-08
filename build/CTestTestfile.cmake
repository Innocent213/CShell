# CMake generated Testfile for 
# Source directory: /home/ubuntuschule/Documents/C_Programme/CShell/lib/cJSON
# Build directory: /home/ubuntuschule/Documents/C_Programme/CShell/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(cJSON_test "/home/ubuntuschule/Documents/C_Programme/CShell/build/cJSON_test")
set_tests_properties(cJSON_test PROPERTIES  _BACKTRACE_TRIPLES "/home/ubuntuschule/Documents/C_Programme/CShell/lib/cJSON/CMakeLists.txt;252;add_test;/home/ubuntuschule/Documents/C_Programme/CShell/lib/cJSON/CMakeLists.txt;0;")
subdirs("tests")
subdirs("fuzzing")
