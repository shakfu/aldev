# Tracker sequencer tests

# Tracker library sources (built as part of tests for now)
set(TRACKER_SOURCES
    ${PSND_ROOT_DIR}/source/core/tracker/tracker_model.c
    ${PSND_ROOT_DIR}/source/core/tracker/tracker_plugin.c
    ${PSND_ROOT_DIR}/source/core/tracker/tracker_plugin_notes.c
    ${PSND_ROOT_DIR}/source/core/tracker/tracker_engine.c
    ${PSND_ROOT_DIR}/source/core/tracker/tracker_audio.c
    ${PSND_ROOT_DIR}/source/core/tracker/tracker_view.c
    ${PSND_ROOT_DIR}/source/core/tracker/tracker_view_theme.c
    ${PSND_ROOT_DIR}/source/core/tracker/tracker_view_undo.c
    ${PSND_ROOT_DIR}/source/core/tracker/tracker_view_clipboard.c
    ${PSND_ROOT_DIR}/source/core/tracker/tracker_view_json.c
    ${PSND_ROOT_DIR}/source/core/tracker/tracker_view_terminal.c
    ${PSND_ROOT_DIR}/source/core/tracker/tracker_midi_import.cpp
)

# Create tracker static library
add_library(tracker STATIC ${TRACKER_SOURCES})
target_include_directories(tracker PUBLIC
    ${PSND_ROOT_DIR}/source/core/tracker
    ${PSND_ROOT_DIR}/source/core/include
    ${PSND_ROOT_DIR}/source/core/shared
    ${PSND_ROOT_DIR}/source/core/loki
    ${PSND_ROOT_DIR}/source/thirdparty/midifile/include
)
# Link against shared library for audio integration, loki for JSON parser, midifile for MIDI import
target_link_libraries(tracker PUBLIC shared libloki midifile)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tracker PRIVATE -Wall -Wextra -Wno-unused-function)
endif()

# Helper macro for tracker tests
macro(add_tracker_test TEST_NAME)
    add_executable(test_tracker_${TEST_NAME} test_${TEST_NAME}.c)
    target_include_directories(test_tracker_${TEST_NAME} PRIVATE
        ${PSND_ROOT_DIR}/source/core/tracker
        ${PSND_ROOT_DIR}/source/core/include
        ${PSND_ROOT_DIR}/source/core/tests
        ${PSND_ROOT_DIR}/source/testing
    )
    target_link_libraries(test_tracker_${TEST_NAME} PRIVATE tracker test_framework)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(test_tracker_${TEST_NAME} PRIVATE -Wno-unused-function)
    endif()
    add_test(
        NAME tracker_${TEST_NAME}_tests
        COMMAND $<TARGET_FILE:test_tracker_${TEST_NAME}>
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(tracker_${TEST_NAME}_tests PROPERTIES LABELS "unit;tracker")
endmacro()

# Add tracker tests
add_tracker_test(plugin_notes)
add_tracker_test(audio)
add_tracker_test(midi_import)

# Tracker demo (interactive, not a test)
add_executable(tracker_demo tracker_demo.c)
target_include_directories(tracker_demo PRIVATE
    ${PSND_ROOT_DIR}/source/core/tracker
    ${PSND_ROOT_DIR}/source/core/include
)
target_link_libraries(tracker_demo PRIVATE tracker)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tracker_demo PRIVATE -Wno-unused-function)
endif()
