cmake_minimum_required(VERSION 3.18)
project(REST VERSION 2.0.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# C++ standard – honour whatever ROOT was compiled with (17 minimum)
# ---------------------------------------------------------------------------
find_package(ROOT 6.24 REQUIRED COMPONENTS Core RIO Tree Graf)
include(${ROOT_USE_FILE})

execute_process(COMMAND root-config --has-cxx20 OUTPUT_VARIABLE _has20 OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND root-config --has-cxx17 OUTPUT_VARIABLE _has17 OUTPUT_STRIP_TRAILING_WHITESPACE)

if(_has20 STREQUAL "yes")
    set(CMAKE_CXX_STANDARD 20)
elseif(_has17 STREQUAL "yes")
    set(CMAKE_CXX_STANDARD 17)
else()
    message(FATAL_ERROR "ROOT must be compiled with C++17 or later.")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
add_compile_options(-fPIC -Wall -Wextra -Wno-unused-parameter)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-Werror -Wno-cast-function-type -Wno-unknown-pragmas)
endif()

# ---------------------------------------------------------------------------
# Install prefix guard – never pollute /usr/local
# ---------------------------------------------------------------------------
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR
   CMAKE_INSTALL_PREFIX STREQUAL "/usr/local")
    if(DEFINED ENV{REST_PATH})
        set(CMAKE_INSTALL_PREFIX "$ENV{REST_PATH}" CACHE PATH "" FORCE)
    else()
        set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "" FORCE)
    endif()
    message(STATUS "Install prefix set to: ${CMAKE_INSTALL_PREFIX}")
endif()

# ---------------------------------------------------------------------------
# External dependencies
# ---------------------------------------------------------------------------
find_package(yaml-cpp REQUIRED)

# ---------------------------------------------------------------------------
# ROOT dictionary output directory (shared by all sub-projects)
# ---------------------------------------------------------------------------
set(ROOT_DICT_OUTPUT_DIR "${CMAKE_BINARY_DIR}/rootdict")
file(MAKE_DIRECTORY ${ROOT_DICT_OUTPUT_DIR})

# ---------------------------------------------------------------------------
# Subdirectories
#   source/tools   – logging, filesystem helpers  (no ROOT dict needed)
#   source/core    – TRestEvent, TRestMetadata, TRestRun, TRestManager
#   libraries/raw  – TRestRawSignalEvent  (opt-in via REST_RAW)
#   source/bin     – executables
# ---------------------------------------------------------------------------
add_subdirectory(source/tools)
add_subdirectory(source/core)

option(REST_RAW "Build the raw-signal library" ON)
if(REST_RAW)
    add_subdirectory(libraries/raw)
endif()

add_subdirectory(source/bin)

# Create thisREST.sh
#include(thisREST)

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== REST ${PROJECT_VERSION} ===")
message(STATUS "  Install : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  ROOT    : ${ROOT_VERSION}")
message(STATUS "  REST_RAW: ${REST_RAW}")
message(STATUS "")
