
cmake_minimum_required(VERSION 3.4.3)
project(multiconnect)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/multiconnect.obj
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/test.eds
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND edl -O2 ${CMAKE_CURRENT_SOURCE_DIR}/test.eds -o ${CMAKE_CURRENT_BINARY_DIR}/multiconnect.obj
    COMMENT "--- EDL Compiling : test.eds ---"
    DEPENDS edl reader.edl)

# Now build our tools
add_executable(multiconnect 
    test.c
    reader.edl
    ${CMAKE_CURRENT_BINARY_DIR}/multiconnect.obj
    )

set_target_properties(multiconnect PROPERTIES FOLDER tests)


add_test(testingcontention multiconnect)
