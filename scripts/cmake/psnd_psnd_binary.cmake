include_guard(GLOBAL)

add_executable(psnd_bin
    ${PSND_ROOT_DIR}/src/main.c
    ${PSND_ROOT_DIR}/src/repl.c
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
