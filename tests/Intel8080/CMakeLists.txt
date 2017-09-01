
cmake_minimum_required(VERSION 3.4.3)
project(Intel8080)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pin8080.obj
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/../../chips/Accurate/i8080.edl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND edl -O2 ${CMAKE_CURRENT_SOURCE_DIR}/../../chips/Accurate/i8080.edl -o ${CMAKE_CURRENT_BINARY_DIR}/pin8080.obj
    COMMENT "--- EDL Compiling : pin accurate i8080.edl ---"
    DEPENDS edl)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/step8080.obj
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/../../chips/Simplified/i8080.edl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND edl -O2 ${CMAKE_CURRENT_SOURCE_DIR}/../../chips/Simplified/i8080.edl -o ${CMAKE_CURRENT_BINARY_DIR}/step8080.obj
    COMMENT "--- EDL Compiling : step based i8080.edl ---"
    DEPENDS edl)

# Now build our tests
add_executable(test8080 
    testHarness.c
    ${CMAKE_CURRENT_BINARY_DIR}/pin8080.obj
    )

add_executable(testStep8080 
    testHarnessSimplified.c
    ${CMAKE_CURRENT_BINARY_DIR}/step8080.obj
    )

add_custom_command(TARGET test8080 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/test.bin ${CMAKE_CURRENT_BINARY_DIR}
    )
add_custom_command(TARGET testStep8080 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/test.bin ${CMAKE_CURRENT_BINARY_DIR}
    )

set_target_properties(test8080 PROPERTIES FOLDER tests)
set_target_properties(testStep8080 PROPERTIES FOLDER tests)

add_test(TestPin8080 test8080)
add_test(TestStep8080 testStep8080)
