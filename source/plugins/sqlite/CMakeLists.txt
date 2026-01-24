# source/plugins/sqlite/CMakeLists.txt - SQLite FTS5 search index plugin

message(STATUS "Building SQLite FTS5 search index plugin")

# Require system SQLite with FTS5 support
find_package(SQLite3 REQUIRED)
message(STATUS "Found system SQLite3: ${SQLite3_VERSION}")

# Check for FTS5 support
include(CheckCSourceRuns)
set(CMAKE_REQUIRED_LIBRARIES SQLite::SQLite3)
check_c_source_runs("
    #include <sqlite3.h>
    int main() {
        sqlite3 *db;
        if (sqlite3_open(\":memory:\", &db) != SQLITE_OK) return 1;
        int rc = sqlite3_exec(db,
            \"CREATE VIRTUAL TABLE t USING fts5(content)\",
            0, 0, 0);
        sqlite3_close(db);
        return (rc == SQLITE_OK) ? 0 : 1;
    }
" SQLITE_FTS5_WORKS)
unset(CMAKE_REQUIRED_LIBRARIES)

if(NOT SQLITE_FTS5_WORKS)
    message(FATAL_ERROR
        "System SQLite3 does not have FTS5 support.\n"
        "Install a SQLite3 build with FTS5 enabled, or disable the plugin:\n"
        "  cmake .. -DBUILD_PLUGIN_SQLITE=OFF"
    )
endif()

message(STATUS "System SQLite3 has FTS5 support")

# FTS plugin library (core C implementation)
add_library(psnd_fts STATIC
    fts.c
)

target_include_directories(psnd_fts PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(psnd_fts PUBLIC
    SQLite::SQLite3
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(psnd_fts PRIVATE -Wall -Wextra -pedantic)
endif()

# FTS Lua bindings library
add_library(psnd_fts_lua STATIC
    lua_fts.c
)

target_include_directories(psnd_fts_lua PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(psnd_fts_lua PUBLIC
    psnd_fts
    lua
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(psnd_fts_lua PRIVATE -Wall -Wextra -pedantic)
endif()

# Export for linking into libloki
set(PSND_FTS_LIBRARY psnd_fts_lua CACHE INTERNAL "FTS plugin library with Lua bindings")
set(PSND_FTS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "FTS plugin include dir")

# Tests
if(BUILD_TESTING)
    add_executable(test_fts test_fts.c)
    target_link_libraries(test_fts PRIVATE psnd_fts test_framework)
    add_test(NAME test_fts COMMAND test_fts)
endif()
