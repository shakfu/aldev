# TR7 language tests

# Helper macro for tr7 tests
macro(add_tr7_test TEST_NAME)
    add_executable(test_tr7_${TEST_NAME}
        test_${TEST_NAME}.c
        ${PSND_ROOT_DIR}/source/testing/test_framework.c
    )
    target_link_libraries(test_tr7_${TEST_NAME} PRIVATE
        tr7
        shared
        m
    )
    target_include_directories(test_tr7_${TEST_NAME} PRIVATE
        ${PSND_ROOT_DIR}/source/testing
        ${PSND_ROOT_DIR}/source/thirdparty/tr7
        ${PSND_ROOT_DIR}/source/core/shared
        ${PSND_ROOT_DIR}/source/core/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../impl
    )
    add_test(
        NAME tr7_${TEST_NAME}_tests
        COMMAND $<TARGET_FILE:test_tr7_${TEST_NAME}>
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(tr7_${TEST_NAME}_tests PROPERTIES LABELS "unit")
endmacro()

# Add all tr7 tests
add_tr7_test(music)
