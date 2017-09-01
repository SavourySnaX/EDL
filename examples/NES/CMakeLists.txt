macro(EDLCompile outName inName symName)
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${outName}
		MAIN_DEPENDENCY ${inName}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND edl -g -O0 -s ${symName} ${inName} -o ${CMAKE_CURRENT_BINARY_DIR}/${outName}
		COMMENT "--- EDL Compiling : ${outName} ---"
		DEPENDS edl)
endmacro(EDLCompile)

if (${glfw3_FOUND} AND ${OPENGL_FOUND})

	cmake_minimum_required(VERSION 3.4.3)
	project(NES)

	include_directories(${OPENGL_INCLUDE_DIR})
	include_directories(${CMAKE_SOURCE_DIR}/include/)

	EDLCompile(rp2a03.obj ../../chips/Accurate/rp2a03.edl MAIN_)
	EDLCompile(rp2c02.obj ../../chips/Accurate/rp2c02.edl PPU_)

	# Now build our tools
	add_executable(NES 
		nes.c gui/debugger.c
		gui/debugger.h gui/font.h
		${CMAKE_CURRENT_BINARY_DIR}/rp2a03.obj
		${CMAKE_CURRENT_BINARY_DIR}/rp2c02.obj
		)

	target_link_libraries(NES ${OPENGL_gl_LIBRARY})
	target_link_libraries(NES glfw)

	set_target_properties(NES PROPERTIES FOLDER examples)

	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/rp2a03.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/rp2c02.obj)
endif()
