# source/core/CMakeLists.txt - Core psnd libraries and binary

# Save core directory path for use in functions called from other directories
set(PSND_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "Core source directory")

# ==============================================================================
# Shared Library
# ==============================================================================
set(LIBSHARED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/context.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/repl_commands.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/audio/tsf_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/audio/csound_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/midi/midi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/midi/events.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/link/link.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/async/shared_async.c
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/music/music_theory.c
)

add_library(shared STATIC ${LIBSHARED_SOURCES})
add_library(shared::shared ALIAS shared)

target_include_directories(shared
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/shared
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${PSND_ROOT_DIR}/source/thirdparty/TinySoundFont
        ${PSND_ROOT_DIR}/source/thirdparty/miniaudio
        ${PSND_ROOT_DIR}/source/thirdparty/link-3.1.5/extensions/abl_link/include
)

target_link_libraries(shared PUBLIC libremidi abl_link uv_a Threads::Threads)

if(BUILD_CSOUND_BACKEND)
    target_compile_definitions(shared PRIVATE BUILD_CSOUND_BACKEND)
    target_include_directories(shared PRIVATE
        ${PSND_ROOT_DIR}/source/thirdparty/csound-6.18.1/include
        ${PSND_ROOT_DIR}/source/thirdparty/csound-6.18.1/H
        ${CMAKE_BINARY_DIR}/source/thirdparty/csound-6.18.1/include
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

# ==============================================================================
# Loki Library (built after languages are discovered)
# ==============================================================================
# Note: This function is called from source/langs/CMakeLists.txt after language discovery
function(psnd_build_loki)
    set(LOKI_LIBRARY_TYPE STATIC)
    if(LOKI_BUILD_SHARED)
        set(LOKI_LIBRARY_TYPE SHARED)
    endif()

    # Core loki sources
    set(LOKI_CORE_SOURCES
        ${PSND_CORE_DIR}/loki/core.c
        ${PSND_CORE_DIR}/loki/lua.c
        ${PSND_CORE_DIR}/loki/editor.c
        ${PSND_CORE_DIR}/loki/syntax.c
        ${PSND_CORE_DIR}/loki/indent.c
        ${PSND_CORE_DIR}/loki/languages.c
        ${PSND_CORE_DIR}/loki/selection.c
        ${PSND_CORE_DIR}/loki/search.c
        ${PSND_CORE_DIR}/loki/modal.c
        ${PSND_CORE_DIR}/loki/event.c
        ${PSND_CORE_DIR}/loki/renderer.c
        ${PSND_CORE_DIR}/loki/command.c
        ${PSND_CORE_DIR}/loki/command/file.c
        ${PSND_CORE_DIR}/loki/command/basic.c
        ${PSND_CORE_DIR}/loki/command/goto.c
        ${PSND_CORE_DIR}/loki/command/substitute.c
        ${PSND_CORE_DIR}/loki/command/link.c
        ${PSND_CORE_DIR}/loki/command/csd.c
        ${PSND_CORE_DIR}/loki/command/export.c
        ${PSND_CORE_DIR}/loki/command/loop.c
        ${PSND_CORE_DIR}/loki/terminal.c
        ${PSND_CORE_DIR}/loki/undo.c
        ${PSND_CORE_DIR}/loki/buffers.c
        ${PSND_CORE_DIR}/loki/link.c
        ${PSND_CORE_DIR}/loki/csound.c
        ${PSND_CORE_DIR}/loki/export.c
        ${PSND_CORE_DIR}/loki/midi_export.cpp
        ${PSND_CORE_DIR}/loki/lang_bridge.c
        ${PSND_CORE_DIR}/loki/live_loop.c
        ${PSND_CORE_DIR}/loki/repl_launcher.c
        ${PSND_CORE_DIR}/loki/repl_helpers.c
        ${PSND_CORE_DIR}/loki/serialize.c
        ${PSND_CORE_DIR}/loki/session.c
        ${PSND_CORE_DIR}/loki/cli.c
        ${PSND_CORE_DIR}/loki/host.c
        ${PSND_CORE_DIR}/loki/json.c
        ${PSND_CORE_DIR}/loki/jsonrpc.c
    )

    # Collect language-specific register sources
    psnd_collect_lang_sources()

    add_library(libloki ${LOKI_LIBRARY_TYPE}
        ${LOKI_CORE_SOURCES}
        ${PSND_ALL_LANG_REGISTER_SOURCES}
    )

    target_include_directories(libloki PUBLIC
        ${PSND_CORE_DIR}/include
        ${PSND_CORE_DIR}
        ${CMAKE_BINARY_DIR}/generated
        ${PSND_ROOT_DIR}/source/thirdparty/link-3.1.5/extensions/abl_link/include
        ${PSND_ROOT_DIR}/source/thirdparty/midifile/include
        ${PSND_ALL_LANG_INCLUDES}
    )

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(libloki PRIVATE -Wall -Wextra -pedantic)
    endif()

    target_link_libraries(libloki PUBLIC
        lua
        Threads::Threads
        ${CMAKE_DL_LIBS}
        abl_link
        midifile
        shared
        ${PSND_ALL_LANG_LINK_LIBRARIES}
    )

    psnd_apply_lang_definitions(libloki)
    set_target_properties(libloki PROPERTIES OUTPUT_NAME "loki")

    if(NOT MSVC)
        target_link_libraries(libloki PUBLIC m)
    endif()
endfunction()

# ==============================================================================
# psnd Binary (built after languages are discovered)
# ==============================================================================
function(psnd_build_binary)
    psnd_collect_lang_sources()

    add_executable(psnd_bin
        ${PSND_CORE_DIR}/main.c
        ${PSND_CORE_DIR}/lang_dispatch.c
        ${PSND_CORE_DIR}/repl.c
        ${PSND_ALL_LANG_REPL_SOURCES}
    )

    set_target_properties(psnd_bin PROPERTIES 
        OUTPUT_NAME "psnd"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )

    if(NOT MSVC)
        add_custom_command(TARGET psnd_bin POST_BUILD
            COMMAND strip $<TARGET_FILE:psnd_bin>
            COMMENT "Stripping psnd binary"
        )
    endif()

    target_include_directories(psnd_bin PRIVATE
        ${PSND_ROOT_DIR}
        ${PSND_CORE_DIR}/include
        ${PSND_CORE_DIR}
        ${CMAKE_BINARY_DIR}/generated
        ${PSND_ALL_LANG_INCLUDES}
    )

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(psnd_bin PRIVATE -Wall -Wextra -pedantic)
    endif()

    target_link_libraries(psnd_bin PRIVATE libloki)
    psnd_apply_lang_definitions(psnd_bin)
endfunction()

# ==============================================================================
# Tests (built after psnd binary)
# ==============================================================================
function(psnd_build_core_tests)
    if(NOT BUILD_TESTING)
        return()
    endif()

    add_test(NAME psnd_version COMMAND $<TARGET_FILE:psnd_bin> --version)
    add_test(NAME psnd_help COMMAND $<TARGET_FILE:psnd_bin> --help)

    add_subdirectory(${PSND_CORE_DIR}/tests/loki ${CMAKE_BINARY_DIR}/tests/loki)
    add_subdirectory(${PSND_CORE_DIR}/tests/shared ${CMAKE_BINARY_DIR}/tests/shared)
    add_subdirectory(${PSND_CORE_DIR}/tests/cli ${CMAKE_BINARY_DIR}/tests/cli)
endfunction()
