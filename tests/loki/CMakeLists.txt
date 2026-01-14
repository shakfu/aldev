# loki editor tests

# Unit tests for core functionality
add_executable(test_core test_core.c)
target_include_directories(test_core PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)
target_link_libraries(test_core PRIVATE libloki test_framework)
add_test(NAME test_core COMMAND test_core)

# Integration tests for file I/O
add_executable(test_file_io test_file_io.c)
target_include_directories(test_file_io PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)
target_link_libraries(test_file_io PRIVATE libloki test_framework)
add_test(NAME test_file_io COMMAND test_file_io)

# Integration tests for Lua API
add_executable(test_lua_api test_lua_api.c)
target_include_directories(test_lua_api PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_lua_api PRIVATE libloki test_framework)
add_test(NAME test_lua_api COMMAND test_lua_api)

# Unit tests for language registration helpers
add_executable(test_lang_registration test_lang_registration.c)
target_include_directories(test_lang_registration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_lang_registration PRIVATE libloki test_framework)
add_test(NAME test_lang_registration COMMAND test_lang_registration)

# Unit tests for modal editing (vim-like modes)
add_executable(test_modal test_modal.c)
target_include_directories(test_modal PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_modal PRIVATE libloki test_framework)
add_test(NAME test_modal COMMAND test_modal)

# Unit tests for syntax highlighting
add_executable(test_syntax test_syntax.c)
target_include_directories(test_syntax PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_syntax PRIVATE libloki test_framework)
add_test(NAME test_syntax COMMAND test_syntax)

# Unit tests for search functionality
add_executable(test_search test_search.c)
target_include_directories(test_search PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_search PRIVATE libloki test_framework)
add_test(NAME test_search COMMAND test_search)

# Unit tests for buffer management
add_executable(test_buffers test_buffers.c)
target_include_directories(test_buffers PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_buffers PRIVATE libloki test_framework)
add_test(NAME test_buffers COMMAND test_buffers)

# Unit tests for auto-indentation
add_executable(test_indent test_indent.c)
target_include_directories(test_indent PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_indent PRIVATE libloki test_framework)
add_test(NAME test_indent COMMAND test_indent)

# Unit tests for undo/redo system
add_executable(test_undo test_undo.c)
target_include_directories(test_undo PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_undo PRIVATE libloki test_framework)
add_test(NAME test_undo COMMAND test_undo)

# Unit tests for selection and clipboard operations
add_executable(test_selection test_selection.c)
target_include_directories(test_selection PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_selection PRIVATE libloki test_framework)
add_test(NAME test_selection COMMAND test_selection)

# Unit tests for vim-style command mode
add_executable(test_command test_command.c)
target_include_directories(test_command PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_command PRIVATE libloki test_framework)
add_test(NAME test_command COMMAND test_command)

# Unit tests for terminal buffer operations
add_executable(test_terminal test_terminal.c)
target_include_directories(test_terminal PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_terminal PRIVATE libloki test_framework)
add_test(NAME test_terminal COMMAND test_terminal)

# Unit tests for row operations
add_executable(test_row_operations test_row_operations.c)
target_include_directories(test_row_operations PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(test_row_operations PRIVATE libloki test_framework)
add_test(NAME test_row_operations COMMAND test_row_operations)
