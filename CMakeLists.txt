cmake_minimum_required(VERSION 2.8)

project(cellatom)

set(cellatom_CXX_SRCS
	# Just compile the Pegmatite .cc files into the program, don't bother
	# building a separate library.
	Pegmatite/ast.cc
	Pegmatite/parser.cc
	ast.cc
	compiler.cc
	interpreter.cc
	main.cc
)
set(LLVM_LIBS
	all
)

# Define the cellatom program that we will build
add_executable(cellatom ${cellatom_CXX_SRCS})
# We're using pegmatite in the RTTI mode
add_definitions(-DUSE_RTTI=1)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")


find_program(LLVM_CONFIG NAMES llvm-config DOC "Path to llvm-config utility")
if (${LLVM_CONFIG} STREQUAL "LLVM_CONFIG-NOTFOUND")
	message(SEND_ERROR "llvm-config not found, please manually set path with -DLLVM_CONFIG")
endif()
message(STATUS "Using llvm-config: ${LLVM_CONFIG}")

# Define LLVM version macros so that we can support multiple versions in the source.
exec_program(${LLVM_CONFIG}
	ARGS --version
	OUTPUT_VARIABLE LLVM_VER)
exec_program(${LLVM_CONFIG}
	ARGS --cxxflags
	OUTPUT_VARIABLE LLVM_CXXFLAGS)
exec_program(${LLVM_CONFIG}
	ARGS --libs ${LLVM_LIBS}
	OUTPUT_VARIABLE LLVM_LIBS_FLAGS)
exec_program(${LLVM_CONFIG}
	ARGS --ldflags
	OUTPUT_VARIABLE LLVM_LDFLAGS)
exec_program(${LLVM_CONFIG}
	ARGS --system-libs
	OUTPUT_VARIABLE LLVM_SYSTEMLIBS)
string(REGEX REPLACE "([0-9]*).([0-9]*).*" "-DLLVM_MAJOR=\\1 -DLLVM_MINOR=\\2"  LLVM_VERSION "${LLVM_VER}")

add_custom_command(OUTPUT runtime.bc
	COMMAND clang -c -emit-llvm ${CMAKE_CURRENT_SOURCE_DIR}/runtime.c -o runtime.bc -O0
	MAIN_DEPENDENCY runtime.c)
add_custom_target(build_runtime DEPENDS runtime.bc)
add_dependencies(cellatom build_runtime)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_CXXFLAGS} ${LLVM_VERSION}")
target_link_libraries(cellatom ${LLVM_LIBS_FLAGS})
target_link_libraries(cellatom ${LLVM_SYSTEMLIBS})
set(CMAKE_EXE_LINKER_FLAGS "${LLVM_LDFLAGS} ${CMAKE_EXE_LINKER_FLAGS}")





option(BUILD_DOCUMENTATION "Use Doxygen to create the HTML based API documentation" OFF)
if(BUILD_DOCUMENTATION)
	FIND_PACKAGE(Doxygen)
	if (NOT DOXYGEN_FOUND)
		message(FATAL_ERROR
			"Doxygen is needed to build the documentation. Please install it correctly")
	endif()
	#-- Configure the Template Doxyfile for our specific project
	configure_file(Doxyfile.in
	               ${PROJECT_BINARY_DIR}/Doxyfile @ONLY IMMEDIATE)
	#-- Add a custom target to run Doxygen when ever the project is built
	add_custom_target (Docs ALL
	                   COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/Doxyfile
	                   SOURCES ${PROJECT_BINARY_DIR}/Doxyfile)
endif()

