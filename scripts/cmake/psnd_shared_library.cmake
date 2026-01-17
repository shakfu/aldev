include_guard(GLOBAL)

set(LIBSHARED_SOURCES
    ${PSND_ROOT_DIR}/src/shared/context.c
    ${PSND_ROOT_DIR}/src/shared/repl_commands.c
    ${PSND_ROOT_DIR}/src/shared/audio/tsf_backend.c
    ${PSND_ROOT_DIR}/src/shared/audio/csound_backend.c
    ${PSND_ROOT_DIR}/src/shared/midi/midi.c
    ${PSND_ROOT_DIR}/src/shared/midi/events.c
    ${PSND_ROOT_DIR}/src/shared/link/link.c
    ${PSND_ROOT_DIR}/src/shared/async/shared_async.c
)

add_library(shared STATIC ${LIBSHARED_SOURCES})
add_library(shared::shared ALIAS shared)

set(SHARED_INCLUDE_DIRS
    ${PSND_ROOT_DIR}/src/shared
    ${PSND_ROOT_DIR}/include
)
set(SHARED_PRIVATE_INCLUDE_DIRS
    ${PSND_ROOT_DIR}/thirdparty/TinySoundFont
    ${PSND_ROOT_DIR}/thirdparty/miniaudio
    ${PSND_ROOT_DIR}/thirdparty/link-3.1.5/extensions/abl_link/include
)

target_include_directories(shared
    PUBLIC
        ${SHARED_INCLUDE_DIRS}
    PRIVATE
        ${SHARED_PRIVATE_INCLUDE_DIRS}
)

target_link_libraries(shared PUBLIC libremidi abl_link uv_a Threads::Threads)

if(BUILD_CSOUND_BACKEND)
    target_compile_definitions(shared PRIVATE BUILD_CSOUND_BACKEND)
    target_include_directories(shared PRIVATE
        ${PSND_ROOT_DIR}/thirdparty/csound-6.18.1/include
        ${PSND_ROOT_DIR}/thirdparty/csound-6.18.1/H
        ${CMAKE_BINARY_DIR}/thirdparty/csound-6.18.1/include
    )
    if(APPLE)
        target_link_libraries(shared PRIVATE CsoundLib64-static)
    else()
        target_link_libraries(shared PRIVATE csound64-static)
    endif()
endif()

if(APPLE)
    target_link_libraries(shared PUBLIC
        "-framework CoreMIDI"
        "-framework CoreFoundation"
        "-framework CoreAudio"
        "-framework AudioToolbox"
    )
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(shared PRIVATE _GNU_SOURCE)
    find_package(ALSA)
    if(ALSA_FOUND)
        target_link_libraries(shared PUBLIC ${ALSA_LIBRARIES})
        target_include_directories(shared PRIVATE ${ALSA_INCLUDE_DIRS})
    endif()
    target_link_libraries(shared PUBLIC pthread dl m)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(shared PRIVATE -Wall -Wextra -Wpedantic)
endif()
