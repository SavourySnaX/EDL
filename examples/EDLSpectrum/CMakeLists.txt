if (${glfw3_FOUND} AND ${OPENGL_FOUND})

	cmake_minimum_required(VERSION 3.4.3)
	project(EDLSpectrum)

	include_directories(${OPENGL_INCLUDE_DIR})
	include_directories(${CMAKE_SOURCE_DIR}/include/)

	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cpu_rom_test.obj
		MAIN_DEPENDENCY cpu_rom_test.eds
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND edl -g -O0 cpu_rom_test.eds -o ${CMAKE_CURRENT_BINARY_DIR}/cpu_rom_test.obj
		COMMENT "--- EDL Compiling : cpu_rom_test.eds ---"
		DEPENDS edl chips/spectrum_rom.edl ../../chips/Accurate/z80.edl chips/ULA.edl chips/4116_8.edl chips/74LS157.edl chips/DeadCockroach.edl)

	# Now build our tools
	add_executable(EDLSpectrum 
		Testing.c tape.c gui/debugger.c
		gui/debugger.h gui/font.h
		${CMAKE_CURRENT_BINARY_DIR}/cpu_rom_test.obj
		chips/spectrum_rom.edl ../../chips/Accurate/z80.edl chips/ULA.edl chips/4116_8.edl chips/74LS157.edl chips/DeadCockroach.edl
		)

	target_link_libraries(EDLSpectrum ${OPENGL_gl_LIBRARY})
	target_link_libraries(EDLSpectrum glfw)

    set_target_properties(EDLSpectrum PROPERTIES FOLDER examples)

	source_group(Chips FILES chips/spectrum_rom.edl)
	source_group(Chips FILES ../../chips/Accurate/z80.edl)
	source_group(Chips FILES chips/ULA.edl chips/4116_8.edl)
	source_group(Chips FILES chips/74LS157.edl)
	source_group(Chips FILES chips/DeadCockroach.edl)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/cpu_rom_test.obj)
endif()
