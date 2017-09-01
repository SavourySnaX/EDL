
cmake_minimum_required(VERSION 3.4.3)
project(states)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/states.obj
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/states.edl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND edl -O2 ${CMAKE_CURRENT_SOURCE_DIR}/states.edl -o ${CMAKE_CURRENT_BINARY_DIR}/states.obj
    COMMENT "--- EDL Compiling : states.edl ---"
    DEPENDS edl)

# Now build our tools
add_executable(states 
    test.c
    ${CMAKE_CURRENT_BINARY_DIR}/states.obj
    )

set_target_properties(states PROPERTIES FOLDER tests)


add_test(TestStates states)
