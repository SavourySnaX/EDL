
cmake_minimum_required(VERSION 3.4.3)
project(mb14241)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mb14241.obj
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/../../chips/Accurate/mb14241.edl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND edl -O2 ${CMAKE_CURRENT_SOURCE_DIR}/../../chips/Accurate/mb14241.edl -o ${CMAKE_CURRENT_BINARY_DIR}/mb14241.obj
    COMMENT "--- EDL Compiling : mb14241.edl ---"
    DEPENDS edl)

# Now build our tools
add_executable(mb14241 
    test.c
    ${CMAKE_CURRENT_BINARY_DIR}/mb14241.obj
    )

set_target_properties(mb14241 PROPERTIES FOLDER tests)

add_test(Test14241 mb14241)
