
cmake_minimum_required(VERSION 3.4.3)
project(multialias)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/multialias.obj
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/multialias.edl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND edl -O2 ${CMAKE_CURRENT_SOURCE_DIR}/multialias.edl -o ${CMAKE_CURRENT_BINARY_DIR}/multialias.obj
    COMMENT "--- EDL Compiling : multialias.edl ---"
    DEPENDS edl)

# Now build our tools
add_executable(multialias 
    test.c
    ${CMAKE_CURRENT_BINARY_DIR}/multialias.obj
    )

set_target_properties(multialias PROPERTIES FOLDER tests)


add_test(TestMultiAlias multialias)
