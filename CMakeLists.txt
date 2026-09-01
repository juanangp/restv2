cmake_minimum_required(VERSION 3.18)
project(
  REST
  VERSION 2.0.0
  LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Canonical REST target names
# ---------------------------------------------------------------------------
set(RAW_LIB RestRaw)
set(GEANT4_LIB RestGeant4)
set(DETECTOR_LIB RestDetector)

# ---------------------------------------------------------------------------
# C++ standard - honour whatever ROOT was compiled with (17 minimum)
# ---------------------------------------------------------------------------
set(ROOT_REQUIRED_LIBRARIES
    Core
    RIO
    Tree
    Graf
    Hist
    Physics
    Geom
    Gui 
    Eve
    RGL
    TreePlayer)

find_package(ROOT 6.24 REQUIRED COMPONENTS ${ROOT_REQUIRED_LIBRARIES})
include(${ROOT_USE_FILE})

set(ROOT_TARGET_LIBRARIES "")
foreach(_root_lib IN LISTS ROOT_REQUIRED_LIBRARIES)
  list(APPEND ROOT_TARGET_LIBRARIES ROOT::${_root_lib})
endforeach()

execute_process(
  COMMAND root-config --has-cxx20
  OUTPUT_VARIABLE _has20
  OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
  COMMAND root-config --has-cxx17
  OUTPUT_VARIABLE _has17
  OUTPUT_STRIP_TRAILING_WHITESPACE)

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
# Install prefix guard - never pollute /usr/local
# ---------------------------------------------------------------------------
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR CMAKE_INSTALL_PREFIX STREQUAL "/usr/local")
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
# Automatic dictionary generation helper (no LinkDef/XML required)
# ---------------------------------------------------------------------------
function(rest_attach_dictionary target)
  set(options)
  set(oneValueArgs)
  set(multiValueArgs HEADERS INCLUDE_DIRS)
  cmake_parse_arguments(RAD "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT RAD_HEADERS)
    return()
  endif()

  if(NOT COMMAND ROOT_GENERATE_DICTIONARY)
    message(FATAL_ERROR "ROOT_GENERATE_DICTIONARY is not available.")
  endif()

  set(_dict_options "")
  foreach(_inc IN LISTS RAD_INCLUDE_DIRS)
    list(APPEND _dict_options -I${_inc})
  endforeach()

  ROOT_GENERATE_DICTIONARY(G__${target}
    ${RAD_HEADERS}
    MODULE ${target}
    OPTIONS ${_dict_options})
endfunction()

# ---------------------------------------------------------------------------
# Subdirectories activation via unifed flags
# ---------------------------------------------------------------------------
add_subdirectory(source/tools)
add_subdirectory(source/core)

option(${RAW_LIB} "Build the raw-signal library" ON)
if(${RAW_LIB})
  add_subdirectory(libraries/raw)
endif()

option(${DETECTOR_LIB} "Build the detector library" ON)
if(${DETECTOR_LIB})
  add_subdirectory(libraries/detector)
endif()

option(${GEANT4_LIB} "Build the geant4 library" ON)
if(${GEANT4_LIB})
  add_subdirectory(libraries/geant4)
endif()

# Nuevo Flag para el paquete de simulación ejecutable restG4
option(REST_G4 "Build the optional restG4 package application" ON)
if(REST_G4)
  add_subdirectory(packages/restG4)
endif()

add_subdirectory(source/bin)

# Create thisREST.sh
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(thisREST)

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== REST ${PROJECT_VERSION} ===")
message(STATUS "  Install : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  ROOT    : ${ROOT_VERSION}")
message(STATUS "  ${RAW_LIB}: ${${RAW_LIB}}")
message(STATUS "  ${DETECTOR_LIB}: ${${DETECTOR_LIB}}")
message(STATUS "  ${GEANT4_LIB}: ${${GEANT4_LIB}}")
message(STATUS "  REST_G4 : ${REST_G4}")
message(STATUS "")

