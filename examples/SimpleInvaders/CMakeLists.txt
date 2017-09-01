if (${glfw3_FOUND} AND ${OPENGL_FOUND})

	cmake_minimum_required(VERSION 3.4.3)
	project(SimpleInvaders)

	include_directories(${OPENGL_INCLUDE_DIR})
	include_directories(${CMAKE_SOURCE_DIR}/include/)

	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/i8080.obj
		MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/../../chips/Simplified/i8080.edl
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND edl -g -O0 ${CMAKE_CURRENT_SOURCE_DIR}/../../chips/Simplified/i8080.edl -o ${CMAKE_CURRENT_BINARY_DIR}/i8080.obj
		COMMENT "--- EDL Compiling : i8080.edl ---"
		DEPENDS edl)
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mb14241.obj
		MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/../../chips/Accurate/mb14241.edl
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND edl -g -O0 ${CMAKE_CURRENT_SOURCE_DIR}/../../chips/Accurate/mb14241.edl -o ${CMAKE_CURRENT_BINARY_DIR}/mb14241.obj
		COMMENT "--- EDL Compiling : mb14241.edl ---"
		DEPENDS edl)


	# Now build our tools
	add_executable(SimpleInvaders 
		invaders.c
		${CMAKE_CURRENT_BINARY_DIR}/i8080.obj
		${CMAKE_CURRENT_BINARY_DIR}/mb14241.obj
		${CMAKE_CURRENT_SOURCE_DIR}/../../chips/Simplified/i8080.edl
		${CMAKE_CURRENT_SOURCE_DIR}/../../chips/Accurate/mb14241.edl
		)

	target_link_libraries(SimpleInvaders ${OPENGL_gl_LIBRARY})
	target_link_libraries(SimpleInvaders glfw)

	set_target_properties(SimpleInvaders PROPERTIES FOLDER examples)

	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/i8080.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/mb14241.obj)
endif()
