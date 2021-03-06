cmake_minimum_required(VERSION 3.4.3)
set (CMAKE_CXX_STANDARD 14)
set (CMAKE_C_STANDARD 99)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
project(edl)

find_package(LLVM REQUIRED CONFIG)
find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)
# below are used by samples (samples won't be generated if the dependencies are missing)
find_package(glfw3 3 CONFIG)
find_package(OpenGL)
find_package(OpenAL)

enable_testing()

if (UNIX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -no-pie")
endif()
if (APPLE)
	add_definitions(-DEDL_PLATFORM_APPLE)
endif()
if (MSVC)
	add_definitions(-DEDL_PLATFORM_MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
endif()
if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
	add_definitions(-DEDL_PLATFORM_ARM)
endif()
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

BISON_TARGET(MyParser src/edl.y
	${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
	DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.hpp)
FLEX_TARGET(MyLexer src/edl.l
	${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp)
ADD_FLEX_BISON_DEPENDENCY(MyLexer MyParser)

set(AST_OPERATORS_HEADERS src/ast/opBinary.h src/ast/opCast.h src/ast/opRotation.h src/ast/opAssignment.h)
set(AST_OPERANDS_HEADERS src/ast/opNum.h src/ast/opIdent.h src/ast/opMapping.h src/ast/opPartial.h)
set(AST_STATEMENT_STATE_HEADERS src/ast/stateDecl.h src/ast/statesDecl.h src/ast/stateTest.h src/ast/statePush.h src/ast/statePop.h src/ast/stateJump.h src/ast/stateDefinition.h)
set(AST_STATEMENT_HEADERS src/ast/variableDecl.h src/ast/ifStatement.h src/ast/trigger.h src/ast/handlerDecl.h src/ast/connectDecl.h src/ast/mappingDecl.h src/ast/instruction.h
						  src/ast/execute.h src/ast/affect.h src/ast/affector.h src/ast/extern.h src/ast/funcDecl.h src/ast/funcCall.h src/ast/instance.h src/ast/exchange.h)
set(AST_HEADERS src/ast/integer.h src/ast/string.h src/ast/identifier.h src/ast/block.h src/ast/debugHelpers.h)
set(AST_OPERATORS_CPPS src/ast/opBinary.cpp src/ast/opCast.cpp src/ast/opRotation.cpp src/ast/opAssignment.cpp)
set(AST_OPERANDS_CPPS src/ast/opNum.cpp src/ast/opIdent.cpp src/ast/opMapping.cpp src/ast/opPartial.cpp)
set(AST_STATEMENT_STATE_CPPS src/ast/stateDecl.cpp src/ast/statesDecl.cpp src/ast/stateTest.cpp src/ast/statePush.cpp src/ast/statePop.cpp src/ast/stateJump.cpp src/ast/stateDefinition.cpp)
set(AST_STATEMENT_CPPS src/ast/variableDecl.cpp src/ast/ifStatement.cpp src/ast/trigger.cpp src/ast/handlerDecl.cpp src/ast/connectDecl.cpp src/ast/mappingDecl.cpp src/ast/instruction.cpp
                       src/ast/execute.cpp src/ast/affect.cpp src/ast/affector.cpp src/ast/extern.cpp src/ast/funcDecl.cpp src/ast/funcCall.cpp src/ast/instance.cpp src/ast/exchange.cpp)
set(AST_CPPS src/ast/integer.cpp src/ast/string.cpp src/ast/identifier.cpp src/ast/block.cpp src/ast/debugHelpers.cpp)
set(HEADERS src/ast.h src/generator.h src/globalContext.h src/bitvariable.h src/optPasses.h src/nodes.h)
set(GRAMMARS src/edl.l src/edl.y)
set(CPPS src/main.cpp src/generator.cpp src/globalContext.cpp src/bitvariable.cpp src/optPasses.cpp)

add_executable(edl 
	${CPPS}
	${AST_CPPS}
	${AST_OPERATORS_CPPS}
	${AST_OPERANDS_CPPS}
	${AST_STATEMENT_CPPS}
	${AST_STATEMENT_STATE_CPPS}
	${GRAMMARS}
	${HEADERS}
	${AST_HEADERS}
	${AST_OPERATORS_HEADERS}
	${AST_OPERANDS_HEADERS}
	${AST_STATEMENT_HEADERS}
	${AST_STATEMENT_STATE_HEADERS}
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

source_group(Sources FILES ${CPPS})
source_group(Headers FILES ${HEADERS})
source_group(Headers\\ast FILES ${AST_HEADERS})
source_group(Sources\\ast FILES ${AST_CPPS})
source_group(Headers\\ast\\operands FILES ${AST_OPERANDS_HEADERS})
source_group(Sources\\ast\\operands FILES ${AST_OPERANDS_CPPS})
source_group(Headers\\ast\\operators FILES ${AST_OPERATORS_HEADERS})
source_group(Sources\\ast\\operators FILES ${AST_OPERATORS_CPPS})
source_group(Headers\\ast\\statements FILES ${AST_STATEMENT_HEADERS})
source_group(Sources\\ast\\statements FILES ${AST_STATEMENT_CPPS})
source_group(Headers\\ast\\statements\\state FILES ${AST_STATEMENT_STATE_HEADERS})
source_group(Sources\\ast\\statements\\state FILES ${AST_STATEMENT_STATE_CPPS})
source_group(Grammar FILES ${GRAMMARS})
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
add_subdirectory(tests/multialias)
add_subdirectory(tests/bugs)
add_subdirectory(tests/multiconnect)

#
# For debugging
#
#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()
