include_guard(GLOBAL)

set(LIBSHARED_SOURCES
    ${PSND_ROOT_DIR}/source/core/shared/context.c
    ${PSND_ROOT_DIR}/source/core/shared/repl_commands.c
    ${PSND_ROOT_DIR}/source/core/shared/audio/tsf_backend.c
    ${PSND_ROOT_DIR}/source/core/shared/audio/csound_backend.c
    ${PSND_ROOT_DIR}/source/core/shared/audio/fluid_backend.c
    ${PSND_ROOT_DIR}/source/core/shared/midi/midi.c
    ${PSND_ROOT_DIR}/source/core/shared/midi/events.c
    ${PSND_ROOT_DIR}/source/core/shared/link/link.c
    ${PSND_ROOT_DIR}/source/core/shared/async/shared_async.c
    ${PSND_ROOT_DIR}/source/core/shared/music/music_theory.c
)

add_library(shared STATIC ${LIBSHARED_SOURCES})
add_library(shared::shared ALIAS shared)

set(SHARED_INCLUDE_DIRS
    ${PSND_ROOT_DIR}/source/core/shared
    ${PSND_ROOT_DIR}/source/core/include
)
set(SHARED_PRIVATE_INCLUDE_DIRS
    ${PSND_ROOT_DIR}/source/thirdparty/TinySoundFont
    ${PSND_ROOT_DIR}/source/thirdparty/miniaudio
    ${PSND_ROOT_DIR}/source/thirdparty/link-3.1.5/extensions/abl_link/include
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
        ${PSND_ROOT_DIR}/source/thirdparty/csound-6.18.1/include
        ${PSND_ROOT_DIR}/source/thirdparty/csound-6.18.1/H
        ${CMAKE_BINARY_DIR}/thirdparty/csound-6.18.1/include
    )
    if(APPLE)
        target_link_libraries(shared PRIVATE CsoundLib64-static)
    else()
        target_link_libraries(shared PRIVATE csound64-static)
    endif()
endif()

if(BUILD_FLUID_BACKEND)
    target_compile_definitions(shared PRIVATE BUILD_FLUID_BACKEND)
    target_include_directories(shared PRIVATE
        ${PSND_ROOT_DIR}/source/thirdparty/fluidsynth/include
        ${CMAKE_BINARY_DIR}/thirdparty/fluidsynth/include
    )
    target_link_libraries(shared PRIVATE libfluidsynth)
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
