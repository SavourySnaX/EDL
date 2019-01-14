
cmake_minimum_required(VERSION 3.4.3)
project(bugs)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/bugs.obj
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/bugs.edl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND edl -g -O0 ${CMAKE_CURRENT_SOURCE_DIR}/bugs.edl -o ${CMAKE_CURRENT_BINARY_DIR}/bugs.obj
    COMMENT "--- EDL Compiling : bugs.edl ---"
    DEPENDS edl)

# Now build our tools
add_executable(bugs 
    test.c
    ${CMAKE_CURRENT_BINARY_DIR}/bugs.obj
    )

set_target_properties(bugs PROPERTIES FOLDER tests)


add_test(CheckBugs bugs)
