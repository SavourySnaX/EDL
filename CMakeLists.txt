cmake_minimum_required(VERSION 3.4.3)
set (CMAKE_CXX_STANDARD 11)
set (CMAKE_C_STANDARD 99)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
project(edl)

find_package(LLVM REQUIRED CONFIG)
find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)
# below are used by samples (samples won't be generated if the dependencies are missing)
find_package(glfw3 3.2 CONFIG)
find_package(OpenGL)

enable_testing()

if (UNIX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")
endif()
if (MSVC)
	add_definitions(-DEDL_PLATFORM_MSVC)
endif()
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

BISON_TARGET(MyParser src/edl.y
	${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
	DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.hpp)
FLEX_TARGET(MyLexer src/edl.l
	${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp)
ADD_FLEX_BISON_DEPENDENCY(MyLexer MyParser)

# Now build our tools
add_executable(edl 
	src/main.cpp src/generator.cpp
	src/ast.h src/generator.h
	src/edl.l src/edl.y
	${FLEX_MyLexer_OUTPUTS}
	${BISON_MyParser_OUTPUTS} 
	)

target_include_directories(edl PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(edl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(edl PUBLIC ${LLVM_INCLUDE_DIRS})
target_compile_definitions(edl PUBLIC ${LLVM_DEFINITIONS})

if (WIN32)
	target_compile_definitions(edl PUBLIC -DYY_NO_UNISTD_H)
	add_definitions(/wd4996 /wd4624 /wd4291 /wd4141)
	if(MSVC)
		# Force to always compile with W1 (too many warnings with llvm headers otherwise)
		if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
			string(REGEX REPLACE "/W[0-4]" "/W1" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
		else()
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W1")
		endif()
	endif()

endif(WIN32)

source_group(Grammar FILES src/edl.l src/edl.y)
source_group(Generated FILES ${FLEX_MyLexer_OUTPUTS} ${BISON_MyParser_OUTPUTS})

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs Core MC Support MCParser BinaryFormat Analysis BitReader BitWriter Coroutines Coverage ipo IRReader InstCombine Instrumentation LTO Linker ObjCARCopts Object Passes ProfileData ScalarOpts Target TransformUtils Option MCJIT ${LLVM_ALL_TARGETS})

# Link against LLVM libraries
target_link_libraries(edl ${llvm_libs})

add_subdirectory(examples/2600)
add_subdirectory(examples/c64)
add_subdirectory(examples/EDLSpectrum)
add_subdirectory(examples/NES)
add_subdirectory(examples/SimpleInvaders)
add_subdirectory(examples/Invaders)
add_subdirectory(examples/zx80)
add_subdirectory(examples/Vic20)
add_subdirectory(examples/ZXSpectrum)
add_subdirectory(examples/Language/NamedExecute)
add_subdirectory(examples/Language/MultiTest)
add_subdirectory(examples/Language/HelloWorld)
add_subdirectory(examples/Language/Memory)
add_subdirectory(tests/74154)
add_subdirectory(tests/mb14241)
add_subdirectory(tests/Intel8080)
add_subdirectory(tests/contention)
add_subdirectory(tests/states)

#
# For debugging
#
#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
