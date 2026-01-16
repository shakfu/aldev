# CLI integration tests

# Play command integration tests
add_executable(test_play_command
    test_play_command.c
    ${CMAKE_SOURCE_DIR}/tests/test_framework.c
)

target_include_directories(test_play_command PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
)

# Pass the path to the psnd binary
target_compile_definitions(test_play_command PRIVATE
    PSND_BINARY="$<TARGET_FILE:psnd_bin>"
)

# Link only what we need (the test runs psnd as a subprocess)
target_link_libraries(test_play_command PRIVATE test_framework)

# This test depends on psnd being built
add_dependencies(test_play_command psnd_bin)

add_test(
    NAME cli_play_command_tests
    COMMAND $<TARGET_FILE:test_play_command>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(cli_play_command_tests PROPERTIES LABELS "integration")
