# source/plugins/CMakeLists.txt - Optional plugin modules

# SQLite FTS5 search index plugin
option(BUILD_PLUGIN_SQLITE "Build SQLite FTS5 search index plugin" OFF)
if(BUILD_PLUGIN_SQLITE)
    add_subdirectory(sqlite)
endif()
