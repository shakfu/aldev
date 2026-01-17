include_guard(GLOBAL)

set(LIBALDA_SOURCES
    # Implementation files
    ${PSND_ROOT_DIR}/src/lang/alda/impl/tokens.c
    ${PSND_ROOT_DIR}/src/lang/alda/impl/error.c
    ${PSND_ROOT_DIR}/src/lang/alda/impl/scanner.c
    ${PSND_ROOT_DIR}/src/lang/alda/impl/ast.c
    ${PSND_ROOT_DIR}/src/lang/alda/impl/parser.c
    ${PSND_ROOT_DIR}/src/lang/alda/impl/context.c
    ${PSND_ROOT_DIR}/src/lang/alda/impl/instruments.c
    ${PSND_ROOT_DIR}/src/lang/alda/impl/interpreter.c
    ${PSND_ROOT_DIR}/src/lang/alda/impl/attributes.c
    ${PSND_ROOT_DIR}/src/lang/alda/impl/scala.c
    # Backends
    ${PSND_ROOT_DIR}/src/lang/alda/backends/midi_backend.c
    ${PSND_ROOT_DIR}/src/lang/alda/backends/tsf_backend_wrapper.c
    ${PSND_ROOT_DIR}/src/lang/alda/backends/csound_backend.c
    # Scheduler
    ${PSND_ROOT_DIR}/src/lang/alda/scheduler.c
    ${PSND_ROOT_DIR}/src/lang/alda/async.c
)

add_library(alda STATIC ${LIBALDA_SOURCES})
add_library(alda::alda ALIAS alda)

target_include_directories(alda
    PUBLIC
        ${PSND_ROOT_DIR}/include
        ${PSND_ROOT_DIR}/src/lang/alda/include
    PRIVATE
        ${PSND_ROOT_DIR}/src/shared
        ${PSND_ROOT_DIR}/thirdparty/TinySoundFont
        ${PSND_ROOT_DIR}/thirdparty/miniaudio
)

target_link_libraries(alda PUBLIC shared libremidi)

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
