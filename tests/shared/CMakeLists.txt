# Shared service tests

# Test for Ableton Link functionality
# NOTE: Link tests can hang due to network discovery - excluded from CTest
# Run manually: ./build/tests/shared/test_link
add_executable(test_link test_link.c)
target_include_directories(test_link PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/shared
    ${CMAKE_SOURCE_DIR}/tests
)
target_link_libraries(test_link PRIVATE shared test_framework m)
# Excluded from ctest - run manually if needed

# Test for shared MIDI event buffer
add_executable(test_midi_events test_midi_events.c)
target_include_directories(test_midi_events PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/shared
    ${CMAKE_SOURCE_DIR}/tests
)
target_link_libraries(test_midi_events PRIVATE shared test_framework m)
add_test(
    NAME shared_midi_events_tests
    COMMAND $<TARGET_FILE:test_midi_events>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(shared_midi_events_tests PROPERTIES LABELS "unit")

# Test for MIDI export (requires libloki for export functions)
add_executable(test_midi_export test_midi_export.c)
target_include_directories(test_midi_export PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/shared
    ${CMAKE_SOURCE_DIR}/tests
)
target_link_libraries(test_midi_export PRIVATE libloki test_framework m)
add_test(
    NAME shared_midi_export_tests
    COMMAND $<TARGET_FILE:test_midi_export>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(shared_midi_export_tests PROPERTIES LABELS "unit")
