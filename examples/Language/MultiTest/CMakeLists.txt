
cmake_minimum_required(VERSION 3.4.3)
project(multiTest)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/MultiTest.obj
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/MultiTest.edl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND edl -O2 ${CMAKE_CURRENT_SOURCE_DIR}/MultiTest.edl -o ${CMAKE_CURRENT_BINARY_DIR}/MultiTest.obj
    COMMENT "--- EDL Compiling : MultiTest.edl ---"
    DEPENDS edl)

# Now build our tools
add_executable(multiTest 
    Main.c
    ${CMAKE_CURRENT_BINARY_DIR}/MultiTest.obj
    )

set_target_properties(multiTest PROPERTIES FOLDER examples/language)

