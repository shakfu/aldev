# Shared service tests

# Test for Ableton Link functionality
# NOTE: Link tests can hang due to network discovery - excluded from CTest
# Run manually: ./build/tests/shared/test_link
add_executable(test_link test_link.c)
target_include_directories(test_link PRIVATE
    ${PSND_ROOT_DIR}/source/core/include
    ${PSND_ROOT_DIR}/source/core/shared
    ${PSND_ROOT_DIR}/source/testing
)
target_link_libraries(test_link PRIVATE shared test_framework m)
# Excluded from ctest - run manually if needed

# Test for shared MIDI event buffer
add_executable(test_midi_events test_midi_events.c)
target_include_directories(test_midi_events PRIVATE
    ${PSND_ROOT_DIR}/source/core/include
    ${PSND_ROOT_DIR}/source/core/shared
    ${PSND_ROOT_DIR}/source/testing
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
    ${PSND_ROOT_DIR}/source/core/include
    ${PSND_ROOT_DIR}/source/core/shared
    ${PSND_ROOT_DIR}/source/core
    ${PSND_ROOT_DIR}/source/testing
    ${CMAKE_BINARY_DIR}/generated
)
target_link_libraries(test_midi_export PRIVATE libloki test_framework m)
# Apply LANG_* definitions so struct sizes match library
psnd_apply_lang_definitions(test_midi_export)
add_test(
    NAME shared_midi_export_tests
    COMMAND $<TARGET_FILE:test_midi_export>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(shared_midi_export_tests PROPERTIES LABELS "unit")

# Test for TSF (TinySoundFont) audio backend
add_executable(test_tsf_backend test_tsf_backend.c)
target_include_directories(test_tsf_backend PRIVATE
    ${PSND_ROOT_DIR}/source/core/include
    ${PSND_ROOT_DIR}/source/core/shared
    ${PSND_ROOT_DIR}/source/testing
)
target_link_libraries(test_tsf_backend PRIVATE shared test_framework m)
add_test(
    NAME shared_tsf_backend_tests
    COMMAND $<TARGET_FILE:test_tsf_backend>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(shared_tsf_backend_tests PROPERTIES LABELS "unit")

# Test for Csound audio backend
add_executable(test_csound_backend test_csound_backend.c)
target_include_directories(test_csound_backend PRIVATE
    ${PSND_ROOT_DIR}/source/core/include
    ${PSND_ROOT_DIR}/source/core/shared
    ${PSND_ROOT_DIR}/source/testing
)
target_link_libraries(test_csound_backend PRIVATE shared test_framework m)
if(BUILD_CSOUND_BACKEND)
    target_compile_definitions(test_csound_backend PRIVATE BUILD_CSOUND_BACKEND)
endif()
add_test(
    NAME shared_csound_backend_tests
    COMMAND $<TARGET_FILE:test_csound_backend>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(shared_csound_backend_tests PROPERTIES LABELS "unit")

# Test for FluidSynth audio backend
add_executable(test_fluid_backend test_fluid_backend.c)
target_include_directories(test_fluid_backend PRIVATE
    ${PSND_ROOT_DIR}/source/core/include
    ${PSND_ROOT_DIR}/source/core/shared
    ${PSND_ROOT_DIR}/source/testing
)
target_link_libraries(test_fluid_backend PRIVATE shared test_framework m)
if(BUILD_FLUID_BACKEND)
    target_compile_definitions(test_fluid_backend PRIVATE BUILD_FLUID_BACKEND)
endif()
add_test(
    NAME shared_fluid_backend_tests
    COMMAND $<TARGET_FILE:test_fluid_backend>
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(shared_fluid_backend_tests PROPERTIES LABELS "unit")
