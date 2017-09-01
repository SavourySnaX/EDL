
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
	project(c64)

	include_directories(${OPENGL_INCLUDE_DIR})
	include_directories(${CMAKE_SOURCE_DIR}/include/)

	EDLCompile(m6526_cia0.obj ../../chips/Accurate/m6526.edl CIA0_)
	EDLCompile(m6526_cia1.obj ../../chips/Accurate/m6526.edl CIA1_)
	EDLCompile(m6522_disk_via0.obj ../../chips/Accurate/m6522.edl DISK_VIA0_)
	EDLCompile(m6522_disk_via1.obj ../../chips/Accurate/m6522.edl DISK_VIA1_)
	EDLCompile(m6510.obj ../../chips/Accurate/m6510.edl MAIN_)
	EDLCompile(m6502.obj ../../chips/Accurate/m6502.edl DISK_)

	add_executable(c64 
		c64.c 1541.c gui/debugger.c
		gui/debugger.h gui/font.h
		${CMAKE_CURRENT_BINARY_DIR}/m6526_cia0.obj
		${CMAKE_CURRENT_BINARY_DIR}/m6526_cia1.obj
		${CMAKE_CURRENT_BINARY_DIR}/m6522_disk_via0.obj
		${CMAKE_CURRENT_BINARY_DIR}/m6522_disk_via1.obj
		${CMAKE_CURRENT_BINARY_DIR}/m6510.obj
		${CMAKE_CURRENT_BINARY_DIR}/m6502.obj
		)

	target_link_libraries(c64 ${OPENGL_gl_LIBRARY})
	target_link_libraries(c64 glfw)

	set_target_properties(c64 PROPERTIES FOLDER examples)

	source_group(Chips FILES ../../chips/Accurate/m6522.edl)
	source_group(Chips FILES ../../chips/Accurate/m6526.edl)
	source_group(Chips FILES ../../chips/Accurate/m6510.edl)
	source_group(Chips FILES ../../chips/Accurate/m6502.edl)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/m6526_cia0.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/m6526_cia1.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/m6522_disk_via0.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/m6522_disk_via1.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/m6510.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/m6502.obj)
endif()

