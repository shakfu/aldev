include_guard(GLOBAL)

set(LIBALDA_SOURCES
    ${PSND_ROOT_DIR}/src/alda/tokens.c
    ${PSND_ROOT_DIR}/src/alda/error.c
    ${PSND_ROOT_DIR}/src/alda/scanner.c
    ${PSND_ROOT_DIR}/src/alda/ast.c
    ${PSND_ROOT_DIR}/src/alda/parser.c
    ${PSND_ROOT_DIR}/src/alda/context.c
    ${PSND_ROOT_DIR}/src/alda/instruments.c
    ${PSND_ROOT_DIR}/src/alda/midi_backend.c
    ${PSND_ROOT_DIR}/src/alda/tsf_backend_wrapper.c
    ${PSND_ROOT_DIR}/src/alda/csound_backend.c
    ${PSND_ROOT_DIR}/src/alda/scheduler.c
    ${PSND_ROOT_DIR}/src/alda/interpreter.c
    ${PSND_ROOT_DIR}/src/alda/attributes.c
    ${PSND_ROOT_DIR}/src/alda/async.c
    ${PSND_ROOT_DIR}/src/alda/scala.c
)

add_library(alda STATIC ${LIBALDA_SOURCES})
add_library(alda::alda ALIAS alda)

target_include_directories(alda
    PUBLIC
        ${PSND_ROOT_DIR}/include
    PRIVATE
        ${PSND_ROOT_DIR}/src/shared
        ${PSND_ROOT_DIR}/thirdparty/TinySoundFont
        ${PSND_ROOT_DIR}/thirdparty/miniaudio
)

target_link_libraries(alda PUBLIC shared libremidi uv_a)

if(APPLE)
    target_link_libraries(alda PUBLIC
        "-framework CoreMIDI"
        "-framework CoreFoundation"
        "-framework CoreAudio"
        "-framework AudioToolbox"
    )
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(alda PRIVATE _GNU_SOURCE)
    find_package(ALSA)
    if(ALSA_FOUND)
        target_link_libraries(alda PUBLIC ${ALSA_LIBRARIES})
        target_include_directories(alda PRIVATE ${ALSA_INCLUDE_DIRS})
    endif()
    target_link_libraries(alda PUBLIC pthread dl m)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(alda PRIVATE -Wall -Wextra -Wpedantic)
endif()
