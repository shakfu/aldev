include_guard(GLOBAL)

# Bog language library - Prolog-based music live coding

# Core Bog implementation (parser, scheduler, livecoding)
set(LIBBOG_CORE_SOURCES
    ${PSND_ROOT_DIR}/src/lang/bog/impl/bog.c
    ${PSND_ROOT_DIR}/src/lang/bog/impl/builtins.c
    ${PSND_ROOT_DIR}/src/lang/bog/impl/scheduler.c
    ${PSND_ROOT_DIR}/src/lang/bog/impl/livecoding.c
)

add_library(bog STATIC ${LIBBOG_CORE_SOURCES})
add_library(bog::bog ALIAS bog)

target_include_directories(bog
    PUBLIC
        ${PSND_ROOT_DIR}/include
        ${PSND_ROOT_DIR}/src/lang/bog/impl
    PRIVATE
        ${PSND_ROOT_DIR}/src/shared
)

target_link_libraries(bog PUBLIC shared)

if(UNIX)
    target_link_libraries(bog PUBLIC m)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bog PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_compile_definitions(bog PUBLIC LANG_BOG)
