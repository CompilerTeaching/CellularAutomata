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
	instrumentation
	irreader
	mcjit
	linker
	lto
	nativecodegen
	scalaropts
	support
	transformutils
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
exec_program(${LLVM_CONFIG}
	ARGS --libdir
	OUTPUT_VARIABLE LLVM_LIBDIR)
exec_program(${LLVM_CONFIG}
	ARGS --bindir
	OUTPUT_VARIABLE LLVM_BINDIR)
string(REGEX REPLACE "([0-9]*).([0-9]*).*" "-DLLVM_MAJOR=\\1 -DLLVM_MINOR=\\2"  LLVM_VERSION "${LLVM_VER}")

add_custom_command(OUTPUT runtime.bc
	COMMAND "${LLVM_BINDIR}/clang" -c -emit-llvm ${CMAKE_CURRENT_SOURCE_DIR}/runtime.c -o runtime.bc -O0
	MAIN_DEPENDENCY runtime.c)
add_custom_target(build_runtime DEPENDS runtime.bc)
add_dependencies(cellatom build_runtime)

# Explicitly enable RTTI.  The llvm-config output may add -fno-rtti, but
# Pegmatite needs it.
set(CMAKE_CXX_FLAGS
	"${CMAKE_CXX_FLAGS} ${LLVM_CXXFLAGS} ${LLVM_VERSION} -frtti")
target_link_libraries(cellatom ${LLVM_LIBS_FLAGS})
# llvm-config only gained a --system-libs flag in 3.5
if (LLVM_VER VERSION_GREATER 3.4)
	string(STRIP ${LLVM_SYSTEMLIBS} LLVM_SYSTEMLIBS)
	if ("x${LLVM_SYSTEMLIBS}" STREQUAL "x")
	else()
		target_link_libraries(cellatom ${LLVM_SYSTEMLIBS})
	endif()
endif()
set(CMAKE_EXE_LINKER_FLAGS "${LLVM_LDFLAGS} ${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,${LLVM_LIBDIR}")

# Make sure that we use the LLVM path as an rpath so that we can dynamically
# link to LLVM.  Don't let CMake specify its own rpath
set(CMAKE_SKIP_RPATH true)




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

option(ENABLE_TESTS "Enable the tests")

if(ENABLE_TESTS)
	enable_testing()
	add_subdirectory("Tests")
endif()

