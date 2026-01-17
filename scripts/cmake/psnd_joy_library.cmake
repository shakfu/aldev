include_guard(GLOBAL)

set(LIBJOY_SOURCES
    # Implementation files
    ${PSND_ROOT_DIR}/src/lang/joy/impl/joy_runtime.c
    ${PSND_ROOT_DIR}/src/lang/joy/impl/joy_parser.c
    ${PSND_ROOT_DIR}/src/lang/joy/impl/joy_primitives.c
    # Music theory
    ${PSND_ROOT_DIR}/src/lang/joy/music/music_notation.c
    ${PSND_ROOT_DIR}/src/lang/joy/music/music_context.c
    ${PSND_ROOT_DIR}/src/lang/joy/music/music_theory.c
    # MIDI
    ${PSND_ROOT_DIR}/src/lang/joy/midi/joy_midi_backend.c
    ${PSND_ROOT_DIR}/src/lang/joy/midi/midi_primitives.c
    ${PSND_ROOT_DIR}/src/lang/joy/midi/joy_async.c
)

add_library(joy STATIC ${LIBJOY_SOURCES})
add_library(joy::joy ALIAS joy)

target_include_directories(joy
    PUBLIC
        ${PSND_ROOT_DIR}/include
        ${PSND_ROOT_DIR}/src/lang/joy/impl
        ${PSND_ROOT_DIR}/src/lang/joy/music
        ${PSND_ROOT_DIR}/src/lang/joy/midi
    PRIVATE
        ${PSND_ROOT_DIR}/src/shared
)

target_link_libraries(joy PUBLIC shared libremidi)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(joy PRIVATE -Wall -Wextra -Wpedantic)
endif()
