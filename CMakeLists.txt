cmake_minimum_required(VERSION 3.18)
project(psnd_project C CXX)

set(PSND_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND CMAKE_MODULE_PATH "${PSND_ROOT_DIR}/scripts/cmake")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# C++ is needed for libremidi (alda dependency)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(LOKI_BUILD_SHARED "Build libloki as a shared library" OFF)
option(ENABLE_LTO_IPO "Enable Link-time interprocedural optimization" OFF)
option(BUILD_CSOUND_BACKEND "Build with Csound synthesis backend (requires libsndfile)" OFF)
option(BUILD_TESTING "Build tests (default on)" ON)
option(BUILD_WEB_HOST "Build web server host with mongoose" OFF)
option(LUA_SANDBOX "Sandbox Lua: disable os, io, debug, load*" OFF)
option(PSND_ENABLE_ASAN "Enable AddressSanitizer for memory error detection" OFF)
option(PSND_ENABLE_COVERAGE "Enable code coverage instrumentation (gcov/lcov)" OFF)

# AddressSanitizer configuration
if(PSND_ENABLE_ASAN)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        set(ASAN_FLAGS "-fsanitize=address -fno-omit-frame-pointer -g")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ASAN_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ASAN_FLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
        message(STATUS "AddressSanitizer enabled")
    else()
        message(WARNING "AddressSanitizer not supported with this compiler")
    endif()
endif()

# Code coverage configuration
if(PSND_ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        set(COVERAGE_FLAGS "--coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_FLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
        message(STATUS "Code coverage enabled (use lcov/gcov to generate reports)")
    else()
        message(WARNING "Code coverage not supported with this compiler")
    endif()
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache in ${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

if(ENABLE_LTO_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_lto_supported OUTPUT check_lto_ipo_error)
    if(ipo_lto_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
        message(STATUS "IPO / LTO enabled")
    else()
        message(STATUS "IPO / LTO not supported: <${check_lto_ipo_error}>")
    endif()
endif()

find_package(Threads REQUIRED)

# Enable testing at the top level
if(BUILD_TESTING)
    enable_testing()
endif()

# Build everything from source/
add_subdirectory(source)

# ==============================================================================
# Install Target
# ==============================================================================
# Usage: cmake --install build [--prefix /usr/local]

include(GNUInstallDirs)

# Install the psnd binary
install(TARGETS psnd_bin
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install default configuration files
install(DIRECTORY ${CMAKE_SOURCE_DIR}/.psnd/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/psnd
    FILES_MATCHING
    PATTERN "*.lua"
    PATTERN "languages/*.lua"
    PATTERN "keybindings/*.lua"
    PATTERN "modules/*.lua"
    PATTERN "themes/*.lua"
)

# Print install summary
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Binary: ${CMAKE_INSTALL_BINDIR}/psnd")
message(STATUS "  Config: ${CMAKE_INSTALL_DATADIR}/psnd/")
