
cmake_minimum_required(VERSION 3.4.3)
project(testcontention)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/testcontention.obj
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/test.eds
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND edl -O2 ${CMAKE_CURRENT_SOURCE_DIR}/test.eds -o ${CMAKE_CURRENT_BINARY_DIR}/testcontention.obj
    COMMENT "--- EDL Compiling : test.eds ---"
    DEPENDS edl rom0.edl rom1.edl reader.edl)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/simplecontention.obj
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/simple.edl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND edl -O2 ${CMAKE_CURRENT_SOURCE_DIR}/simple.edl -o ${CMAKE_CURRENT_BINARY_DIR}/simplecontention.obj
    COMMENT "--- EDL Compiling : simple.edl ---"
    DEPENDS edl)

# Now build our tools
add_executable(testcontention 
    test.c
    rom0.edl rom1.edl reader.edl
    ${CMAKE_CURRENT_BINARY_DIR}/testcontention.obj
    )
add_executable(testsimple 
    simple.c
    ${CMAKE_CURRENT_BINARY_DIR}/simplecontention.obj
    )

set_target_properties(testcontention PROPERTIES FOLDER tests)
set_target_properties(testsimple PROPERTIES FOLDER tests)


add_test(testingcontention testcontention)
add_test(testingsimple testsimple)
