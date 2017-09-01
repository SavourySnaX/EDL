
cmake_minimum_required(VERSION 3.4.3)
project(namedExecute)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/named.obj
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/named.edl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND edl -O2 ${CMAKE_CURRENT_SOURCE_DIR}/named.edl -o ${CMAKE_CURRENT_BINARY_DIR}/named.obj
    COMMENT "--- EDL Compiling : named.edl ---"
    DEPENDS edl)

# Now build our tools
add_executable(namedExecute 
    Main.c
    ${CMAKE_CURRENT_BINARY_DIR}/named.obj
    )

set_target_properties(namedExecute PROPERTIES FOLDER examples/language)

