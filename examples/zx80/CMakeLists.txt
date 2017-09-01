cmake_minimum_required(VERSION 3.4.3)
project(EDLZX80)

add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cpu_rom_test.obj
		MAIN_DEPENDENCY cpu_rom_test.eds
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND edl -g -O0 cpu_rom_test.eds -o ${CMAKE_CURRENT_BINARY_DIR}/cpu_rom_test.obj
		COMMENT "--- EDL Compiling : cpu_rom_test.eds ---"
		DEPENDS edl)

	# Now build our tools
add_executable(EDLZX80 
		zx80.c
		${CMAKE_CURRENT_BINARY_DIR}/cpu_rom_test.obj
		)

set_target_properties(EDLZX80 PROPERTIES FOLDER examples)

source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/cpu_rom_test.obj)
