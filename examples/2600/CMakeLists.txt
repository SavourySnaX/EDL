
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
	project(2600)

	if (${OPENAL_FOUND})
		include_directories(${OPENAL_INCLUDE_DIR})
		add_definitions(-DEDL_PLATFORM_OPENAL)
	endif()
	include_directories(${OPENGL_INCLUDE_DIR})
	include_directories(${CMAKE_SOURCE_DIR}/include/)

	EDLCompile(m6507.obj ../../chips/Accurate/m6507.edl MAIN_)
	EDLCompile(tia_ntsc.obj ../../chips/Accurate/tia_ntsc.edl TIA_)
	EDLCompile(m6532.obj ../../chips/Accurate/m6532.edl RIOT_)

	# Cart Mappers
	EDLCompile(cart_2k.obj carts/2k.edl CART2K_)
	EDLCompile(cart_4k.obj carts/4k.edl CART4K_)
	EDLCompile(cart_8k.obj carts/8k.edl CART8K_)
	EDLCompile(cart_8kE0.obj carts/8kE0.edl CART8KE0_)
	EDLCompile(cart_12k.obj carts/12k.edl CART12K_)
	EDLCompile(cart_16k.obj carts/16k.edl CART16K_)
	EDLCompile(cart_16kSC.obj carts/16kSC.edl CART16KSC_)
	EDLCompile(cart_32k.obj carts/32k.edl CART32K_)
	EDLCompile(cart_32k3F.obj carts/32k3F.edl CART32K3F_)
	EDLCompile(cart_32kSC.obj carts/32kSC.edl CART32KSC_)

	# Now build our tools
	add_executable(2600 
		2600.c CartIdent.c
		CartIdent.h
		${CMAKE_CURRENT_BINARY_DIR}/m6507.obj
		${CMAKE_CURRENT_BINARY_DIR}/tia_ntsc.obj
		${CMAKE_CURRENT_BINARY_DIR}/m6532.obj
		${CMAKE_CURRENT_BINARY_DIR}/cart_2k.obj
		${CMAKE_CURRENT_BINARY_DIR}/cart_4k.obj
		${CMAKE_CURRENT_BINARY_DIR}/cart_8k.obj
		${CMAKE_CURRENT_BINARY_DIR}/cart_8kE0.obj
		${CMAKE_CURRENT_BINARY_DIR}/cart_12k.obj
		${CMAKE_CURRENT_BINARY_DIR}/cart_16k.obj
		${CMAKE_CURRENT_BINARY_DIR}/cart_16kSC.obj
		${CMAKE_CURRENT_BINARY_DIR}/cart_32k.obj
		${CMAKE_CURRENT_BINARY_DIR}/cart_32k3F.obj
		${CMAKE_CURRENT_BINARY_DIR}/cart_32kSC.obj
		)

	if (${OPENAL_FOUND})
		target_link_libraries(2600 ${OPENAL_LIBRARY})
	endif()
	target_link_libraries(2600 ${OPENGL_gl_LIBRARY})
	target_link_libraries(2600 glfw)

	set_target_properties(2600 PROPERTIES FOLDER examples)

	source_group(Chips FILES ../../chips/Accurate/m6507.edl)
	source_group(Chips FILES ../../chips/Accurate/m6532.edl)
	source_group(Chips FILES ../../chips/Accurate/tia_ntsc.edl)
	source_group(Carts FILES carts/2k.edl)
	source_group(Carts FILES carts/4k.edl)
	source_group(Carts FILES carts/8k.edl)
	source_group(Carts FILES carts/8kE0.edl)
	source_group(Carts FILES carts/12k.edl)
	source_group(Carts FILES carts/16k.edl)
	source_group(Carts FILES carts/16kSC.edl)
	source_group(Carts FILES carts/32k.edl)
	source_group(Carts FILES carts/32k3F.edl)
	source_group(Carts FILES carts/32kSC.edl)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/m6507.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/m6532.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/tia_ntsc.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/cart_2k.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/cart_4k.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/cart_8k.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/cart_8kE0.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/cart_12k.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/cart_16k.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/cart_16kSC.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/cart_32k.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/cart_32k3F.obj)
	source_group(Generated FILES ${CMAKE_CURRENT_BINARY_DIR}/cart_32kSC.obj)
endif()

