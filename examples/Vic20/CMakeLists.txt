
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
	project(vic20)

	if (${OPENAL_FOUND})
		include_directories(${OPENAL_INCLUDE_DIR})
		add_definitions(-DEDL_PLATFORM_OPENAL)
	endif()
	include_directories(${OPENGL_INCLUDE_DIR})
	include_directories(${CMAKE_SOURCE_DIR}/include/)

	EDLCompile(m6522_via0.obj ../../chips/Accurate/m6522.edl VIA0_)
	EDLCompile(m6522_via1.obj ../../chips/Accurate/m6522.edl VIA1_)
	EDLCompile(m6522_disk_via0.obj ../../chips/Accurate/m6522.edl DISK_VIA0_)
	EDLCompile(m6522_disk_via1.obj ../../chips/Accurate/m6522.edl DISK_VIA1_)
	EDLCompile(m6502_main.obj ../../chips/Accurate/m6502.edl MAIN_)
	EDLCompile(m6502_disk.obj ../../chips/Accurate/m6502.edl DISK_)

	add_executable(vic20 
		vic20.c 1541.c gui/debugger.c
		gui/debugger.h gui/font.h
		${CMAKE_CURRENT_BINARY_DIR}/m6522_via0.obj
		${CMAKE_CURRENT_BINARY_DIR}/m6522_via1.obj
		${CMAKE_CURRENT_BINARY_DIR}/m6522_disk_via0.obj
		${CMAKE_CURRENT_BINARY_DIR}/m6522_disk_via1.obj
		${CMAKE_CURRENT_BINARY_DIR}/m6502_main.obj
		${CMAKE_CURRENT_BINARY_DIR}/m6502_disk.obj
		)

	if (${OPENAL_FOUND})
		target_link_libraries(vic20 ${OPENAL_LIBRARY})
	endif()
	target_link_libraries(vic20 ${OPENGL_gl_LIBRARY})
	target_link_libraries(vic20 glfw)

	set_target_properties(vic20 PROPERTIES FOLDER examples)

	source_group(Chips FILES ../../chips/Accurate/m6522.edl)
	source_group(Chips FILES ../../chips/Accurate/m6502.edl)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/m6522_via0.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/m6522_via1.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/m6522_disk_via0.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/m6522_disk_via1.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/m6502_main.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/m6502_disk.obj)
endif()

