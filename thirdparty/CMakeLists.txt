# Thirdparty dependencies for Loki

# Lua 5.5.0 - Embedded scripting language
add_subdirectory(lua-5.5.0)

# libremidi - Cross-platform MIDI I/O (C++ library)
set(LIBREMIDI_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBREMIDI_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(libremidi)

# libuv - Async I/O (used by alda for async playback)
set(LIBUV_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(LIBUV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(libuv)

# alda - Music notation language library
# Note: alda's CMakeLists references TinySoundFont and miniaudio paths
# We need to adjust the include paths for our directory structure
add_subdirectory(alda-midi/lib)

# Make TinySoundFont and miniaudio headers available to alda
target_include_directories(alda PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/TinySoundFont
    ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio
)
