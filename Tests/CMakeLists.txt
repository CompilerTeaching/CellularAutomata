message(STATUS "Adding tests")
file(GLOB TESTS "*.ca")

foreach(TEST ${TESTS})
	get_filename_component(TEST_NAME ${TEST} NAME_WE)
	message(STATUS "Adding test ${TEST_NAME}")
	add_test(${TEST_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/runtest.sh" "${CMAKE_BINARY_DIR}/cellatom" ${TEST} "${LLVM_BINDIR}/FileCheck")
	add_test("${TEST_NAME}_jit" "${CMAKE_CURRENT_SOURCE_DIR}/runtest.sh" "${CMAKE_BINARY_DIR}/cellatom" ${TEST} "${LLVM_BINDIR}/FileCheck" "-j")
	add_test("${TEST_NAME}_jit_O3" "${CMAKE_CURRENT_SOURCE_DIR}/runtest.sh" "${CMAKE_BINARY_DIR}/cellatom" ${TEST} "${LLVM_BINDIR}/FileCheck" "-j" "-O3")
endforeach()

