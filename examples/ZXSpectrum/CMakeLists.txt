if (${glfw3_FOUND} AND ${OPENGL_FOUND})

	cmake_minimum_required(VERSION 3.4.3)
	project(ZXSpectrum)

	include_directories(${OPENGL_INCLUDE_DIR})
	include_directories(${CMAKE_SOURCE_DIR}/include/)

	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pinz80.obj
		MAIN_DEPENDENCY ../../chips/Accurate/z80.edl
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND edl -g -O0 ../../chips/Accurate/z80.edl -o ${CMAKE_CURRENT_BINARY_DIR}/pinz80.obj
		COMMENT "--- EDL Compiling : z80.edl ---"
		DEPENDS edl)
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/stepz80.obj
		MAIN_DEPENDENCY ../../chips/Simplified/z80.edl
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND edl -g -O0 ../../chips/Simplified/z80.edl -o ${CMAKE_CURRENT_BINARY_DIR}/stepz80.obj
		COMMENT "--- EDL Compiling : z80.edl ---"
		DEPENDS edl)

	add_executable(SimpleSpectrum 
		cpusimple.c common.c
		${CMAKE_CURRENT_BINARY_DIR}/stepz80.obj
		)
	add_executable(PinSpectrum 
		cpupin.c common.c
		${CMAKE_CURRENT_BINARY_DIR}/pinz80.obj
		)

	target_link_libraries(SimpleSpectrum ${OPENGL_gl_LIBRARY})
	target_link_libraries(SimpleSpectrum glfw)
	target_link_libraries(PinSpectrum ${OPENGL_gl_LIBRARY})
	target_link_libraries(PinSpectrum glfw)

	set_target_properties(SimpleSpectrum PROPERTIES FOLDER examples)
	set_target_properties(PinSpectrum PROPERTIES FOLDER examples)

	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/stepz80.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/pinz80.obj)
endif()
