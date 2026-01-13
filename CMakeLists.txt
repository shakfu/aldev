cmake_minimum_required(VERSION 3.18)
project(loki C CXX)

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

add_library(libloki ${LOKI_LIBRARY_TYPE}
    src/loki_core.c
    src/loki_lua.c
    src/loki_editor.c
    src/loki_syntax.c
    src/loki_indent.c
    src/loki_languages.c
    src/loki_selection.c
    src/loki_search.c
    src/loki_modal.c
    src/loki_command.c
    src/loki_terminal.c
    src/loki_undo.c
    src/loki_buffers.c
    src/loki_alda.c
    src/loki_link.c
    src/loki_midi_export.cpp
)

target_include_directories(libloki
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LUA_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/alda-midi/lib/include
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

# Unified aldalog binary (editor + REPL + play modes)
# Note: internal target name is alda_bin to avoid conflict with libalda
add_executable(alda_bin
    src/main.c
    src/alda_repl.c
)
set_target_properties(alda_bin PROPERTIES OUTPUT_NAME "aldalog")

# Strip binary by default (reduces size significantly)
if(NOT MSVC)
    add_custom_command(TARGET alda_bin POST_BUILD
        COMMAND strip $<TARGET_FILE:alda_bin>
        COMMENT "Stripping aldalog binary"
    )
endif()

target_include_directories(alda_bin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/alda-midi/lib/include
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
add_test(NAME aldalog_version COMMAND $<TARGET_FILE:alda_bin> --version)
add_test(NAME aldalog_help COMMAND $<TARGET_FILE:alda_bin> --help)

# Test framework library
add_library(test_framework STATIC tests/test_framework.c)
target_include_directories(test_framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Unit tests for core functionality
add_executable(test_core tests/test_core.c)
target_include_directories(test_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
target_link_libraries(test_core PRIVATE libloki test_framework)
add_test(NAME test_core COMMAND test_core)

# Integration tests for file I/O
add_executable(test_file_io tests/test_file_io.c)
target_include_directories(test_file_io PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
target_link_libraries(test_file_io PRIVATE libloki test_framework)
add_test(NAME test_file_io COMMAND test_file_io)

# Integration tests for Lua API
add_executable(test_lua_api tests/test_lua_api.c)
target_include_directories(test_lua_api PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_lua_api PRIVATE libloki test_framework)
add_test(NAME test_lua_api COMMAND test_lua_api)

# Unit tests for language registration helpers
add_executable(test_lang_registration tests/test_lang_registration.c)
target_include_directories(test_lang_registration PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_lang_registration PRIVATE libloki test_framework)
add_test(NAME test_lang_registration COMMAND test_lang_registration)

# Unit tests for modal editing (vim-like modes)
add_executable(test_modal tests/test_modal.c)
target_include_directories(test_modal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_modal PRIVATE libloki test_framework)
add_test(NAME test_modal COMMAND test_modal)

# Unit tests for syntax highlighting
add_executable(test_syntax tests/test_syntax.c)
target_include_directories(test_syntax PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_syntax PRIVATE libloki test_framework)
add_test(NAME test_syntax COMMAND test_syntax)

# Unit tests for search functionality
add_executable(test_search tests/test_search.c)
target_include_directories(test_search PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_search PRIVATE libloki test_framework)
add_test(NAME test_search COMMAND test_search)

# Unit tests for buffer management
add_executable(test_buffers tests/test_buffers.c)
target_include_directories(test_buffers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_buffers PRIVATE libloki test_framework)
add_test(NAME test_buffers COMMAND test_buffers)

# Unit tests for auto-indentation
add_executable(test_indent tests/test_indent.c)
target_include_directories(test_indent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_indent PRIVATE libloki test_framework)
add_test(NAME test_indent COMMAND test_indent)

# Unit tests for undo/redo system
add_executable(test_undo tests/test_undo.c)
target_include_directories(test_undo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_undo PRIVATE libloki test_framework)
add_test(NAME test_undo COMMAND test_undo)

# Unit tests for selection and clipboard operations
add_executable(test_selection tests/test_selection.c)
target_include_directories(test_selection PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_selection PRIVATE libloki test_framework)
add_test(NAME test_selection COMMAND test_selection)

# Unit tests for vim-style command mode
add_executable(test_command tests/test_command.c)
target_include_directories(test_command PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_command PRIVATE libloki test_framework)
add_test(NAME test_command COMMAND test_command)

# Unit tests for terminal buffer operations
add_executable(test_terminal tests/test_terminal.c)
target_include_directories(test_terminal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_terminal PRIVATE libloki test_framework)
add_test(NAME test_terminal COMMAND test_terminal)

# Unit tests for row operations
add_executable(test_row_operations tests/test_row_operations.c)
target_include_directories(test_row_operations PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_row_operations PRIVATE libloki test_framework)
add_test(NAME test_row_operations COMMAND test_row_operations)
