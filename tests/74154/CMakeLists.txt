
cmake_minimum_required(VERSION 3.4.3)
project(74154)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/74154.obj
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/../../chips/Accurate/74154.edl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND edl -O2 ${CMAKE_CURRENT_SOURCE_DIR}/../../chips/Accurate/74154.edl -o ${CMAKE_CURRENT_BINARY_DIR}/74154.obj
    COMMENT "--- EDL Compiling : 74154.edl ---"
    DEPENDS edl)

# Now build our tools
add_executable(74154 
    test.c
    ${CMAKE_CURRENT_BINARY_DIR}/74154.obj
    )

set_target_properties(74154 PROPERTIES FOLDER tests)


add_test(Test74154 74154)
