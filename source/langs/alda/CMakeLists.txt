# Alda language - Declarative music notation language
#
# This CMakeLists.txt is processed by psnd_languages.cmake auto-discovery.
# No modifications to other files are needed to add/remove this language.

# ==============================================================================
# Library Sources
# ==============================================================================
set(ALDA_IMPL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/tokens.c
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/error.c
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/scanner.c
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/ast.c
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/context.c
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/instruments.c
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/interpreter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/attributes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/scala.c
)

set(ALDA_BACKEND_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/midi_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/tsf_backend_wrapper.c
    ${CMAKE_CURRENT_SOURCE_DIR}/backends/csound_backend.c
)

set(ALDA_SCHEDULER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/async.c
)

set(ALDA_ALL_LIB_SOURCES
    ${ALDA_IMPL_SOURCES}
    ${ALDA_BACKEND_SOURCES}
    ${ALDA_SCHEDULER_SOURCES}
)

# ==============================================================================
# Build the Language Library
# ==============================================================================
add_library(alda STATIC ${ALDA_ALL_LIB_SOURCES})
add_library(alda::alda ALIAS alda)

target_include_directories(alda
    PUBLIC
        ${PSND_ROOT_DIR}/source/core/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${PSND_ROOT_DIR}/source/core/shared
        ${PSND_ROOT_DIR}/source/thirdparty/TinySoundFont
        ${PSND_ROOT_DIR}/source/thirdparty/miniaudio
)

target_link_libraries(alda PUBLIC shared libremidi)

# Pass through FluidSynth backend flag
if(BUILD_FLUID_BACKEND)
    target_compile_definitions(alda PRIVATE BUILD_FLUID_BACKEND)
    target_include_directories(alda PRIVATE
        ${PSND_ROOT_DIR}/source/thirdparty/fluidsynth/include
        ${CMAKE_BINARY_DIR}/source/thirdparty/fluidsynth/include
    )
endif()

# Platform-specific dependencies
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

# ==============================================================================
# Register with psnd Language System
# ==============================================================================
psnd_register_language(
    NAME alda
    DISPLAY_NAME "Alda"
    DESCRIPTION "Declarative music notation language"
    COMMANDS alda
    EXTENSIONS .alda
    SOURCES ${ALDA_ALL_LIB_SOURCES}
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    REPL_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/repl.c
        ${CMAKE_CURRENT_SOURCE_DIR}/dispatch.c
    REGISTER_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/register.c
    LINK_LIBRARIES alda
)
