# ---------------------------------------------------------------------------
# libraries/connectors  -  RestConnectors (shared library)
# Processes that transform events of one library into events of another one
# (e.g. raw -> detector). Each connector is built only if the libraries it
# connects are built.
# ---------------------------------------------------------------------------

set(CONNECTORS_SOURCES "")
set(CONNECTORS_HEADERS "")
set(CONNECTORS_DEPS "")

# rest_add_connector(<ClassName> <lib> [<lib> ...]): adds inc/<ClassName>.h and
# src/<ClassName>.cxx when all the given library targets exist.
macro(rest_add_connector CONNECTOR_NAME)
  set(_connector_enabled TRUE)
  foreach(_lib ${ARGN})
    if(NOT TARGET ${_lib})
      set(_connector_enabled FALSE)
    endif()
  endforeach()

  if(_connector_enabled)
    list(APPEND CONNECTORS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/${CONNECTOR_NAME}.cxx)
    list(APPEND CONNECTORS_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/inc/${CONNECTOR_NAME}.h)
    list(APPEND CONNECTORS_DEPS ${ARGN})
  else()
    message(STATUS "${CONNECTORS_LIB}: ${CONNECTOR_NAME} disabled (requires ${ARGN})")
  endif()
endmacro()

rest_add_connector(TRestRawToDetectorSignalProcess ${RAW_LIB} ${DETECTOR_LIB})
rest_add_connector(TRestDetectorHitsToTrackProcess ${DETECTOR_LIB} ${TRACK_LIB})
rest_add_connector(TRestGeant4ToDetectorHitsProcess ${GEANT4_LIB} ${DETECTOR_LIB})
# Legacy connectors not migrated yet:
#   TRestDetectorSignalToRawSignalProcess  (detector, raw)
#   TRestRawReadoutAnalysisProcess         (raw, detector)
#   TRestRawReadoutMetadataProcess         (raw, detector)
#   TRestDetectorHitsToTrackFastProcess    (detector, track)
#   TRestTrackToDetectorHitsProcess        (track, detector)

if(NOT CONNECTORS_SOURCES)
  message(WARNING "${CONNECTORS_LIB}: no connector enabled, the library will not be built")
  return()
endif()

list(REMOVE_DUPLICATES CONNECTORS_DEPS)

add_library(${CONNECTORS_LIB} SHARED ${CONNECTORS_SOURCES})

target_include_directories(
  ${CONNECTORS_LIB} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
                           $<INSTALL_INTERFACE:include>)

target_link_libraries(${CONNECTORS_LIB} PUBLIC RestCore RestTools RestAnalysis ${CONNECTORS_DEPS}
                                               ${ROOT_TARGET_LIBRARIES})

rest_attach_dictionary(${CONNECTORS_LIB}
  HEADERS ${CONNECTORS_HEADERS}
  INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/source/core/inc
    ${CMAKE_SOURCE_DIR}/source/tools/inc
    ${CMAKE_SOURCE_DIR}/source/analysis/inc)

install(
  TARGETS ${CONNECTORS_LIB}
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib/static)
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/lib${CONNECTORS_LIB}_rdict.pcm
    ${CMAKE_CURRENT_BINARY_DIR}/lib${CONNECTORS_LIB}.rootmap
    DESTINATION lib
    OPTIONAL)
install(FILES ${CONNECTORS_HEADERS} DESTINATION include)
