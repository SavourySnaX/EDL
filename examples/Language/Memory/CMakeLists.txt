
cmake_minimum_required(VERSION 3.4.3)
project(memory)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/Memory.obj
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/Memory.edl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND edl -O2 ${CMAKE_CURRENT_SOURCE_DIR}/Memory.edl -o ${CMAKE_CURRENT_BINARY_DIR}/Memory.obj
    COMMENT "--- EDL Compiling : Memory.edl ---"
    DEPENDS edl)

# Now build our tools
add_executable(memory 
    Memory.c
    ${CMAKE_CURRENT_BINARY_DIR}/Memory.obj
    )

set_target_properties(memory PROPERTIES FOLDER examples/language)

