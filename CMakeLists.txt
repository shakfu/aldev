cmake_minimum_required(VERSION 3.18)
project(psnd_project C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# C++ needed for libremidi (alda dependency)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(LOKI_BUILD_SHARED "Build libloki as a shared library" OFF)
option(ENABLE_LTO_IPO "Enable Link-time interprocedural optimization" OFF)
option(BUILD_CSOUND_BACKEND "Build with Csound synthesis backend (requires libsndfile)" OFF)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache in ${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

if (ENABLE_LTO_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_lto_supported OUTPUT check_lto_ipo_error)
    if(ipo_lto_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
        message(STATUS "IPO / LTO enabled")
    else()
        message(STATUS "IPO / LTO not supported: <${check_lto_ipo_error}>")
    endif()
endif()

# Use local Lua 5.5.0 from thirdparty (self-contained build)
set(LUA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua-5.5.0/src)

find_package(Threads REQUIRED)

add_subdirectory(thirdparty)

set(LOKI_LIBRARY_TYPE STATIC)
if (LOKI_BUILD_SHARED)
    set(LOKI_LIBRARY_TYPE SHARED)
endif()

# libalda - Alda music language library (from src/alda/)
set(LIBALDA_SOURCES
    src/alda/tokens.c
    src/alda/error.c
    src/alda/scanner.c
    src/alda/ast.c
    src/alda/parser.c
    src/alda/context.c
    src/alda/instruments.c
    src/alda/midi_backend.c
    src/alda/tsf_backend.c
    src/alda/csound_backend.c
    src/alda/scheduler.c
    src/alda/interpreter.c
    src/alda/attributes.c
    src/alda/async.c
)

add_library(alda STATIC ${LIBALDA_SOURCES})
add_library(alda::alda ALIAS alda)

target_include_directories(alda
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/TinySoundFont
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/miniaudio
)

target_link_libraries(alda PUBLIC libremidi uv_a)

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

# Csound backend for alda (optional)
if(BUILD_CSOUND_BACKEND)
    target_compile_definitions(alda PRIVATE BUILD_CSOUND_BACKEND)
    target_include_directories(alda PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/csound-6.18.1/include
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/csound-6.18.1/H
        ${CMAKE_BINARY_DIR}/thirdparty/csound-6.18.1/include
    )
    if(APPLE)
        target_link_libraries(alda PRIVATE CsoundLib64-static)
    else()
        target_link_libraries(alda PRIVATE csound64-static)
    endif()
endif()

# libloki - Editor library (from src/loki/)
add_library(libloki ${LOKI_LIBRARY_TYPE}
    src/loki/core.c
    src/loki/lua.c
    src/loki/editor.c
    src/loki/syntax.c
    src/loki/indent.c
    src/loki/languages.c
    src/loki/selection.c
    src/loki/search.c
    src/loki/modal.c
    src/loki/command.c
    src/loki/terminal.c
    src/loki/undo.c
    src/loki/buffers.c
    src/loki/alda.c
    src/loki/link.c
    src/loki/midi_export.cpp
)

target_include_directories(libloki
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${LUA_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/link-3.1.5/extensions/abl_link/include
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/midifile/include
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(libloki PRIVATE -Wall -Wextra -pedantic)
endif()

target_link_libraries(libloki
    PUBLIC
        lua
        Threads::Threads
        ${CMAKE_DL_LIBS}
        alda
        abl_link
        midifile
)

if (NOT MSVC)
    target_link_libraries(libloki PUBLIC m)
endif()

# Unified psnd binary (editor + REPL + play modes)
# Note: internal target name is alda_bin to avoid conflict with libalda
add_executable(alda_bin
    src/main.c
    src/repl.c
)
set_target_properties(alda_bin PROPERTIES OUTPUT_NAME "psnd")

# Strip binary by default (reduces size significantly)
if(NOT MSVC)
    add_custom_command(TARGET alda_bin POST_BUILD
        COMMAND strip $<TARGET_FILE:alda_bin>
        COMMENT "Stripping psnd binary"
    )
endif()

target_include_directories(alda_bin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(alda_bin PRIVATE -Wall -Wextra -pedantic)
endif()
target_link_libraries(alda_bin PRIVATE libloki)

add_custom_target(show-config
    COMMAND ${CMAKE_COMMAND} -E echo "Lua include: ${LUA_INCLUDE_DIR} - local 5.5.0"
    COMMAND ${CMAKE_COMMAND} -E echo "Lua libraries: lua - local static"
)

enable_testing()
add_test(NAME psnd_version COMMAND $<TARGET_FILE:alda_bin> --version)
add_test(NAME psnd_help COMMAND $<TARGET_FILE:alda_bin> --help)

# Test framework library (shared by all tests)
add_library(test_framework STATIC tests/test_framework.c)
target_include_directories(test_framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Test subdirectories
add_subdirectory(tests/loki)
add_subdirectory(tests/alda)
