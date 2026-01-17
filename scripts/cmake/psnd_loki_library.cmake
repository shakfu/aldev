include_guard(GLOBAL)

set(LOKI_LIBRARY_TYPE STATIC)
if(LOKI_BUILD_SHARED)
    set(LOKI_LIBRARY_TYPE SHARED)
endif()

# Core loki sources (always built)
set(LOKI_CORE_SOURCES
    ${PSND_ROOT_DIR}/src/loki/core.c
    ${PSND_ROOT_DIR}/src/loki/lua.c
    ${PSND_ROOT_DIR}/src/loki/editor.c
    ${PSND_ROOT_DIR}/src/loki/syntax.c
    ${PSND_ROOT_DIR}/src/loki/indent.c
    ${PSND_ROOT_DIR}/src/loki/languages.c
    ${PSND_ROOT_DIR}/src/loki/selection.c
    ${PSND_ROOT_DIR}/src/loki/search.c
    ${PSND_ROOT_DIR}/src/loki/modal.c
    ${PSND_ROOT_DIR}/src/loki/command.c
    ${PSND_ROOT_DIR}/src/loki/command/file.c
    ${PSND_ROOT_DIR}/src/loki/command/basic.c
    ${PSND_ROOT_DIR}/src/loki/command/goto.c
    ${PSND_ROOT_DIR}/src/loki/command/substitute.c
    ${PSND_ROOT_DIR}/src/loki/command/link.c
    ${PSND_ROOT_DIR}/src/loki/command/csd.c
    ${PSND_ROOT_DIR}/src/loki/command/export.c
    ${PSND_ROOT_DIR}/src/loki/terminal.c
    ${PSND_ROOT_DIR}/src/loki/undo.c
    ${PSND_ROOT_DIR}/src/loki/buffers.c
    ${PSND_ROOT_DIR}/src/loki/link.c
    ${PSND_ROOT_DIR}/src/loki/csound.c
    ${PSND_ROOT_DIR}/src/loki/export.c
    ${PSND_ROOT_DIR}/src/loki/midi_export.cpp
    ${PSND_ROOT_DIR}/src/loki/lang_bridge.c
    ${PSND_ROOT_DIR}/src/loki/repl_launcher.c
    ${PSND_ROOT_DIR}/src/loki/serialize.c
)

# Language-specific register sources (in src/lang/*/register.c)
set(LOKI_LANG_SOURCES)
if(LANG_ALDA)
    list(APPEND LOKI_LANG_SOURCES ${PSND_ROOT_DIR}/src/lang/alda/register.c)
endif()
if(LANG_JOY)
    list(APPEND LOKI_LANG_SOURCES ${PSND_ROOT_DIR}/src/lang/joy/register.c)
endif()
if(LANG_TR7)
    list(APPEND LOKI_LANG_SOURCES ${PSND_ROOT_DIR}/src/lang/tr7/register.c)
endif()
if(LANG_BOG)
    list(APPEND LOKI_LANG_SOURCES ${PSND_ROOT_DIR}/src/lang/bog/register.c)
endif()

add_library(libloki ${LOKI_LIBRARY_TYPE}
    ${LOKI_CORE_SOURCES}
    ${LOKI_LANG_SOURCES}
)

set(LOKI_PUBLIC_INCLUDES
    ${PSND_ROOT_DIR}/include
    ${PSND_ROOT_DIR}/src
    ${PSND_ROOT_DIR}/src/lang/alda/include
    ${PSND_ROOT_DIR}/src/lang/joy/impl
    ${PSND_ROOT_DIR}/src/lang/joy/music
    ${PSND_ROOT_DIR}/src/lang/joy/midi
    ${PSND_ROOT_DIR}/src/lang/bog/impl
    ${PSND_ROOT_DIR}/thirdparty/link-3.1.5/extensions/abl_link/include
    ${PSND_ROOT_DIR}/thirdparty/midifile/include
)

target_include_directories(libloki PUBLIC ${LOKI_PUBLIC_INCLUDES})

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(libloki PRIVATE -Wall -Wextra -pedantic)
endif()

# Core libraries (always linked)
set(LOKI_PUBLIC_LIBS
    lua
    Threads::Threads
    ${CMAKE_DL_LIBS}
    abl_link
    midifile
    shared
)

# Language-specific libraries
if(LANG_ALDA)
    list(APPEND LOKI_PUBLIC_LIBS alda)
    target_compile_definitions(libloki PUBLIC LANG_ALDA=1)
endif()
if(LANG_JOY)
    list(APPEND LOKI_PUBLIC_LIBS joy)
    target_compile_definitions(libloki PUBLIC LANG_JOY=1)
endif()
if(LANG_TR7)
    list(APPEND LOKI_PUBLIC_LIBS tr7)
    target_compile_definitions(libloki PUBLIC LANG_TR7=1)
endif()
if(LANG_BOG)
    list(APPEND LOKI_PUBLIC_LIBS bog)
    target_compile_definitions(libloki PUBLIC LANG_BOG=1)
endif()

target_link_libraries(libloki PUBLIC ${LOKI_PUBLIC_LIBS})

set_target_properties(libloki PROPERTIES OUTPUT_NAME "loki")

if(NOT MSVC)
    target_link_libraries(libloki PUBLIC m)
endif()
