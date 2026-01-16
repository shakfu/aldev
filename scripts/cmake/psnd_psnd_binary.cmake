include_guard(GLOBAL)

# Common infrastructure
set(PSND_COMMON_SOURCES
    ${PSND_ROOT_DIR}/src/lang_dispatch.c
    ${PSND_ROOT_DIR}/src/repl.c
)

# Language-specific REPL and dispatch sources
set(PSND_LANG_SOURCES)
if(LANG_ALDA)
    list(APPEND PSND_LANG_SOURCES
        ${PSND_ROOT_DIR}/src/lang/alda/repl.c
        ${PSND_ROOT_DIR}/src/lang/alda/dispatch.c
    )
endif()
if(LANG_JOY)
    list(APPEND PSND_LANG_SOURCES
        ${PSND_ROOT_DIR}/src/lang/joy/repl.c
        ${PSND_ROOT_DIR}/src/lang/joy/dispatch.c
    )
endif()
if(LANG_TR7)
    list(APPEND PSND_LANG_SOURCES
        ${PSND_ROOT_DIR}/src/lang/tr7/repl.c
        ${PSND_ROOT_DIR}/src/lang/tr7/dispatch.c
    )
endif()

add_executable(psnd_bin
    ${PSND_ROOT_DIR}/src/main.c
    ${PSND_COMMON_SOURCES}
    ${PSND_LANG_SOURCES}
)
set_target_properties(psnd_bin PROPERTIES OUTPUT_NAME "psnd")

if(NOT MSVC)
    add_custom_command(TARGET psnd_bin POST_BUILD
        COMMAND strip $<TARGET_FILE:psnd_bin>
        COMMENT "Stripping psnd binary"
    )
endif()

target_include_directories(psnd_bin PRIVATE
    ${PSND_ROOT_DIR}
    ${PSND_ROOT_DIR}/include
    ${PSND_ROOT_DIR}/src
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(psnd_bin PRIVATE -Wall -Wextra -pedantic)
endif()

target_link_libraries(psnd_bin PRIVATE libloki)
