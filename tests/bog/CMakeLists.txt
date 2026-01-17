# Bog language tests
# Integrated with psnd build system

# Helper macro for bog tests
macro(add_bog_test TEST_NAME)
    add_executable(test_bog_${TEST_NAME} test_${TEST_NAME}.c)
    target_link_libraries(test_bog_${TEST_NAME} PRIVATE bog)
    target_include_directories(test_bog_${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PSND_ROOT_DIR}/src/lang/bog/impl
    )
    if(UNIX)
        target_link_libraries(test_bog_${TEST_NAME} PRIVATE m)
    endif()
    add_test(
        NAME bog_${TEST_NAME}
        COMMAND $<TARGET_FILE:test_bog_${TEST_NAME}>
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(bog_${TEST_NAME} PROPERTIES LABELS "unit")
endmacro()

# Add all bog tests
add_bog_test(parser)
add_bog_test(unify)
add_bog_test(builtins)
add_bog_test(state_manager)
add_bog_test(resolution)
add_bog_test(livecoding)
