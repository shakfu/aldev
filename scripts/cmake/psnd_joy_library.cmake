include_guard(GLOBAL)

set(LIBJOY_SOURCES
    ${PSND_ROOT_DIR}/src/joy/joy_runtime.c
    ${PSND_ROOT_DIR}/src/joy/joy_parser.c
    ${PSND_ROOT_DIR}/src/joy/joy_primitives.c
    ${PSND_ROOT_DIR}/src/joy/joy_midi_backend.c
    ${PSND_ROOT_DIR}/src/joy/midi_primitives.c
    ${PSND_ROOT_DIR}/src/joy/music_notation.c
    ${PSND_ROOT_DIR}/src/joy/music_context.c
    ${PSND_ROOT_DIR}/src/joy/music_theory.c
)

add_library(joy STATIC ${LIBJOY_SOURCES})
add_library(joy::joy ALIAS joy)

target_include_directories(joy
    PUBLIC
        ${PSND_ROOT_DIR}/include
        ${PSND_ROOT_DIR}/src/joy
    PRIVATE
        ${PSND_ROOT_DIR}/src/shared
)

target_link_libraries(joy PUBLIC shared libremidi)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(joy PRIVATE -Wall -Wextra -Wpedantic)
endif()
