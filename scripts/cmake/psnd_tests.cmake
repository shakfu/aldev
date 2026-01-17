include_guard(GLOBAL)

enable_testing()
add_test(NAME psnd_version COMMAND $<TARGET_FILE:psnd_bin> --version)
add_test(NAME psnd_help COMMAND $<TARGET_FILE:psnd_bin> --help)

add_library(test_framework STATIC ${PSND_ROOT_DIR}/tests/test_framework.c)
target_include_directories(test_framework PUBLIC
    ${PSND_ROOT_DIR}/tests
    ${PSND_ROOT_DIR}/include
)

add_subdirectory(${PSND_ROOT_DIR}/tests/loki ${CMAKE_BINARY_DIR}/tests/loki)
if(LANG_ALDA)
    add_subdirectory(${PSND_ROOT_DIR}/tests/alda ${CMAKE_BINARY_DIR}/tests/alda)
endif()
if(LANG_JOY)
    add_subdirectory(${PSND_ROOT_DIR}/tests/joy ${CMAKE_BINARY_DIR}/tests/joy)
endif()
if(LANG_BOG)
    add_subdirectory(${PSND_ROOT_DIR}/tests/bog ${CMAKE_BINARY_DIR}/tests/bog)
endif()
add_subdirectory(${PSND_ROOT_DIR}/tests/shared ${CMAKE_BINARY_DIR}/tests/shared)
add_subdirectory(${PSND_ROOT_DIR}/tests/cli ${CMAKE_BINARY_DIR}/tests/cli)
