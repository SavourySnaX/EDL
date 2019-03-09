if (${glfw3_FOUND} AND ${OPENGL_FOUND})

	cmake_minimum_required(VERSION 3.4.3)
	project(WD65816)

	include_directories(${OPENGL_INCLUDE_DIR})
	include_directories(${CMAKE_SOURCE_DIR}/include/)

	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/test.obj
		MAIN_DEPENDENCY test.eds
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND edl -g -O2 test.eds -o ${CMAKE_CURRENT_BINARY_DIR}/test.obj
		COMMENT "--- EDL Compiling : cpu_rom_test.eds ---"
		DEPENDS edl chips/dummy.edl chips/rom.edl chips/wd65816.edl)

	# Now build our tools
	add_executable(WD65816 
		Testing.c gui/debugger.c
		gui/debugger.h gui/font.h
		${CMAKE_CURRENT_BINARY_DIR}/test.obj
		chips/dummy.edl chips/rom.edl chips/wd65816.edl
		)

	target_link_libraries(WD65816 ${OPENGL_gl_LIBRARY})
	target_link_libraries(WD65816 glfw)

	set_target_properties(WD65816 PROPERTIES FOLDER examples)

	source_group(Chips FILES chips/rom.edl)
	source_group(Chips FILES chips/WD65816.edl)
	source_group(Chips FILES chips/dummy.edl)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/test.obj)
endif()
