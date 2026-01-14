# alda-midi tests

# Interpreter unit tests
add_executable(test_interpreter
    test_interpreter.c
    ${CMAKE_SOURCE_DIR}/tests/test_framework.c
)
target_link_libraries(test_interpreter PRIVATE alda)
target_link_libraries(test_interpreter PRIVATE m)  # for math functions

add_test(
    NAME alda_interpreter_tests
    COMMAND $<TARGET_FILE:test_interpreter>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(alda_interpreter_tests PROPERTIES LABELS "unit")

# Shared suite test runner (Unix only - uses POSIX dirent.h)
if(NOT WIN32)
    add_executable(test_shared_suite test_shared_suite.c)
    target_link_libraries(test_shared_suite PRIVATE alda)
    target_link_libraries(test_shared_suite PRIVATE m)  # for math functions

    # CTest integration
    add_test(
        NAME alda_midi_test_suite
        COMMAND $<TARGET_FILE:test_shared_suite> "${CMAKE_SOURCE_DIR}/tests/alda/shared_suite"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(alda_midi_test_suite PROPERTIES LABELS "integration")
endif()
