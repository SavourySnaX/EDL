
cmake_minimum_required(VERSION 3.4.3)
project(HelloWorld)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/HelloWorld.obj
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/HelloWorld.edl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND edl -O2 ${CMAKE_CURRENT_SOURCE_DIR}/HelloWorld.edl -o ${CMAKE_CURRENT_BINARY_DIR}/HelloWorld.obj
    COMMENT "--- EDL Compiling : HelloWorld.edl ---"
    DEPENDS edl)

# Now build our tools
add_executable(HelloWorld 
    HelloWorld.c
    ${CMAKE_CURRENT_BINARY_DIR}/HelloWorld.obj
    )

set_target_properties(HelloWorld PROPERTIES FOLDER examples/language)

