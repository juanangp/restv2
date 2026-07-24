# ---------------------------------------------------------------------------
# libraries/geant4  -  RestGeant4 (shared library)
# ---------------------------------------------------------------------------

file(GLOB_RECURSE GEANT4_SOURCES src/*.cxx)
file(GLOB_RECURSE GEANT4_HEADERS inc/*.h)

add_library(${GEANT4_LIB} SHARED ${GEANT4_SOURCES})

target_include_directories(
  ${GEANT4_LIB} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
                      $<INSTALL_INTERFACE:include>)

target_link_libraries(${GEANT4_LIB} PUBLIC RestCore RestTools ${ROOT_TARGET_LIBRARIES})

rest_attach_dictionary(${GEANT4_LIB}
  HEADERS ${GEANT4_HEADERS}
  INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/source/core/inc
    ${CMAKE_SOURCE_DIR}/source/tools/inc)

install(
  TARGETS ${GEANT4_LIB}
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib/static)
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/lib${GEANT4_LIB}_rdict.pcm
    ${CMAKE_CURRENT_BINARY_DIR}/lib${GEANT4_LIB}.rootmap
    DESTINATION lib
    OPTIONAL)
install(FILES ${GEANT4_HEADERS} DESTINATION include)
