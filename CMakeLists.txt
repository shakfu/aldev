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

# default on
option(BUILD_TESTING "Build tests (default on)" ON)
option(LANG_ALDA "Include the Alda language" ON)
option(LANG_JOY "Include the Joy language" ON)
option(LANG_TR7 "Include the TR7 Scheme language" ON)
option(LANG_BOG "Include the Bog language" ON)

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

add_subdirectory(${PSND_ROOT_DIR}/thirdparty ${CMAKE_BINARY_DIR}/thirdparty)

# common
include(psnd_shared_library)
include(psnd_loki_library)

# langs
if(LANG_ALDA)
    include(psnd_alda_library)
endif()


if(LANG_JOY)
    include(psnd_joy_library)
endif()

if(LANG_BOG)
    include(psnd_bog_library)
endif()

# binary
include(psnd_psnd_binary)

if(BUILD_TESTING)
    include(psnd_tests)
endif()
