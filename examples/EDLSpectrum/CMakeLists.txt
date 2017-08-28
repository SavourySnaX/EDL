if (${glfw3_FOUND} AND ${OPENGL_FOUND})

	cmake_minimum_required(VERSION 3.4.3)
	project(EDLSpectrum)

	include_directories(${OPENGL_INCLUDE_DIR})

	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cpu_rom_test.obj
		MAIN_DEPENDENCY cpu_rom_test.eds
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND EDLCMakeTest -g -O0 cpu_rom_test.eds -o ${CMAKE_CURRENT_BINARY_DIR}/cpu_rom_test.obj
		COMMENT "--- EDL Compiling : cpu_rom_test.eds ---"
		DEPENDS EDLCMakeTest)

	# Now build our tools
	add_executable(EDLSpectrum 
		Testing.c tape.c gui/debugger.c
		glext.h gui/debugger.h gui/font.h
		${CMAKE_CURRENT_BINARY_DIR}/cpu_rom_test.obj
		)

	target_link_libraries(EDLSpectrum ${OPENGL_gl_LIBRARY})
	target_link_libraries(EDLSpectrum glfw)

	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/cpu_rom_test.obj)
endif()
