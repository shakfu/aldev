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

# Parser unit tests
add_executable(test_parser
    test_parser.c
    ${CMAKE_SOURCE_DIR}/tests/test_framework.c
)
target_link_libraries(test_parser PRIVATE alda)
target_link_libraries(test_parser PRIVATE m)  # for math functions

add_test(
    NAME alda_parser_tests
    COMMAND $<TARGET_FILE:test_parser>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(alda_parser_tests PROPERTIES LABELS "unit")

# Backend smoke tests
add_executable(test_backends
    test_backends.c
    ${CMAKE_SOURCE_DIR}/tests/test_framework.c
)
target_link_libraries(test_backends PRIVATE alda)
target_link_libraries(test_backends PRIVATE m)

# Pass csound backend definition if enabled
if(BUILD_CSOUND_BACKEND)
    target_compile_definitions(test_backends PRIVATE BUILD_CSOUND_BACKEND)
endif()

add_test(
    NAME alda_backend_tests
    COMMAND $<TARGET_FILE:test_backends>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(alda_backend_tests PROPERTIES LABELS "unit")

# Integration tests
add_executable(test_integration
    test_integration.c
    ${CMAKE_SOURCE_DIR}/tests/test_framework.c
)
target_link_libraries(test_integration PRIVATE alda)
target_link_libraries(test_integration PRIVATE m)

add_test(
    NAME alda_integration_tests
    COMMAND $<TARGET_FILE:test_integration>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(alda_integration_tests PROPERTIES LABELS "integration")

# Microtuning tests
add_executable(test_microtuning
    test_microtuning.c
    ${CMAKE_SOURCE_DIR}/tests/test_framework.c
)
target_link_libraries(test_microtuning PRIVATE alda)
target_link_libraries(test_microtuning PRIVATE m)

add_test(
    NAME alda_microtuning_tests
    COMMAND $<TARGET_FILE:test_microtuning>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(alda_microtuning_tests PROPERTIES LABELS "unit")

# Csound microtuning manual test (only when Csound is enabled)
if(BUILD_CSOUND_BACKEND)
    add_executable(test_csound_microtuning
        test_csound_microtuning.c
    )
    target_link_libraries(test_csound_microtuning PRIVATE alda)
    target_link_libraries(test_csound_microtuning PRIVATE m)
    target_compile_definitions(test_csound_microtuning PRIVATE BUILD_CSOUND_BACKEND)
    # Not added to CTest - this is a manual audio test
endif()

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
