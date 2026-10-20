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
set(TRACK_LIB RestTrack)
set(CONNECTORS_LIB RestConnectors)

# ---------------------------------------------------------------------------
# ROOT and C++ standard
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

# C++17 by default (override with -DCMAKE_CXX_STANDARD=20). If ROOT was built with a newer
# standard, the ROOT:: targets raise it through their compile features.
set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard")
if(CMAKE_CXX_STANDARD LESS 17)
  message(FATAL_ERROR "REST requires C++17 or later.")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
add_compile_options(-fPIC -Wall -Wextra -Wno-unused-parameter)
# Off by default: each compiler version adds new warnings, which would break the build
option(REST_WERROR "Treat compiler warnings as errors" OFF)
if(REST_WERROR)
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
# yaml-cpp < 0.8 only exports the target 'yaml-cpp' (or just YAML_CPP_* variables)
if(NOT TARGET yaml-cpp::yaml-cpp)
  if(TARGET yaml-cpp)
    add_library(yaml-cpp::yaml-cpp ALIAS yaml-cpp)
  else()
    add_library(yaml-cpp::yaml-cpp INTERFACE IMPORTED)
    set_target_properties(yaml-cpp::yaml-cpp PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${YAML_CPP_INCLUDE_DIR}"
      INTERFACE_LINK_LIBRARIES "${YAML_CPP_LIBRARIES}")
  endif()
endif()

# ---------------------------------------------------------------------------
# rest_link_registered_libraries(<target> <lib>...)
# REST classes register themselves from static initializers, so a library may be needed
# even if the target uses none of its symbols. Linkers using --as-needed (default on
# Ubuntu and others, or set in LDFLAGS) would drop it, so the target is linked with
# --no-as-needed. Libraries whose target does not exist (disabled) are skipped.
# ---------------------------------------------------------------------------
function(rest_link_registered_libraries target)
  set(_libs "")
  foreach(_lib ${ARGN})
    if(TARGET ${_lib})
      list(APPEND _libs ${_lib})
    endif()
  endforeach()
  target_link_libraries(${target} PRIVATE ${_libs})
  if(NOT APPLE AND NOT WIN32)
    target_link_options(${target} PRIVATE "LINKER:--no-as-needed")
  endif()
endfunction()

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
add_subdirectory(source/analysis)
add_subdirectory(source/core)

option(${GEANT4_LIB} "Build the geant4 library" ON)
if(${GEANT4_LIB})
  add_subdirectory(libraries/geant4)
endif()

option(${RAW_LIB} "Build the raw-signal library" ON)
if(${RAW_LIB})
  add_subdirectory(libraries/raw)
endif()

option(${DETECTOR_LIB} "Build the detector library" ON)
if(${DETECTOR_LIB})
  add_subdirectory(libraries/detector)
endif()

option(${TRACK_LIB} "Build the track library" ON)
if(${TRACK_LIB})
  add_subdirectory(libraries/track)
endif()

# After the libraries it connects, so that their targets exist
option(${CONNECTORS_LIB} "Build the connectors library" ON)
if(${CONNECTORS_LIB})
  add_subdirectory(libraries/connectors)
endif()

# Libraries with registered events/processes, linked by executables and macros
set(REST_LIBRARIES RestTools RestAnalysis RestCore ${RAW_LIB} ${DETECTOR_LIB} ${TRACK_LIB} ${GEANT4_LIB}
                   ${CONNECTORS_LIB})

option(REST_G4 "Build the optional restG4 package application (requires Geant4)" ON)
if(REST_G4)
  find_package(Geant4 QUIET)
  if(NOT Geant4_FOUND)
    message(WARNING "Geant4 not found, restG4 will not be built (set Geant4_DIR or -DREST_G4=OFF)")
    set(REST_G4 OFF)
  endif()
endif()
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
message(STATUS "  ${TRACK_LIB}: ${${TRACK_LIB}}")
message(STATUS "  ${GEANT4_LIB}: ${${GEANT4_LIB}}")
message(STATUS "  ${CONNECTORS_LIB}: ${${CONNECTORS_LIB}}")
message(STATUS "  REST_G4 : ${REST_G4}")
message(STATUS "")

# ---------------------------------------------------------------------------
# Compile macros
# ---------------------------------------------------------------------------
file(GLOB_RECURSE MACROS_SOURCE "${CMAKE_SOURCE_DIR}/source/macros/*.C")
file(GLOB_RECURSE MACROS_LIBRARIES "${CMAKE_SOURCE_DIR}/libraries/**/macros/*.C")
set(ALL_REPOSITORY_MACROS ${MACROS_SOURCE} ${MACROS_LIBRARIES})

if(ALL_REPOSITORY_MACROS)
  install(FILES ${ALL_REPOSITORY_MACROS} DESTINATION macros COMPONENT install)

  file(GLOB_RECURSE ALL_INC_DIRS LIST_DIRECTORIES true "${CMAKE_SOURCE_DIR}/**/inc")

  foreach(MACRO_PATH IN LISTS ALL_REPOSITORY_MACROS)
    get_filename_component(MACRO_WE ${MACRO_PATH} NAME_WE)
    set(MACRO_LIB_NAME "${MACRO_WE}_C")

    add_library(${MACRO_LIB_NAME} SHARED ${MACRO_PATH})

    target_include_directories(${MACRO_LIB_NAME} PRIVATE ${ALL_INC_DIRS})

    set_target_properties(${MACRO_LIB_NAME} PROPERTIES
      PREFIX ""
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/macros_build"
    )

    target_link_libraries(${MACRO_LIB_NAME} PUBLIC ${ROOT_TARGET_LIBRARIES} yaml-cpp::yaml-cpp)
    rest_link_registered_libraries(${MACRO_LIB_NAME} ${REST_LIBRARIES})

    install(TARGETS ${MACRO_LIB_NAME} LIBRARY DESTINATION macros COMPONENT install)
  endforeach()
endif()
