# source/CMakeLists.txt - Main source tree entry point

# Build thirdparty dependencies first
add_subdirectory(thirdparty)

# Build test framework library
add_subdirectory(testing)

# Build optional plugins
add_subdirectory(plugins)

# Build core libraries (shared, loki)
add_subdirectory(core)

# Discover and build language modules
add_subdirectory(langs)
