# ---------------------------------------------------------------------------
# libraries/detector  -  RestDetector (shared library)
# ---------------------------------------------------------------------------

file(GLOB_RECURSE DETECTOR_SOURCES src/*.cxx)
file(GLOB_RECURSE DETECTOR_HEADERS inc/*.h)

add_library(${DETECTOR_LIB} SHARED ${DETECTOR_SOURCES})

target_include_directories(
  ${DETECTOR_LIB} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
                      $<INSTALL_INTERFACE:include>)

target_link_libraries(${DETECTOR_LIB} PUBLIC RestCore RestTools ${ROOT_TARGET_LIBRARIES})

rest_attach_dictionary(${DETECTOR_LIB}
  HEADERS ${DETECTOR_HEADERS}
  INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/source/core/inc
    ${CMAKE_SOURCE_DIR}/source/tools/inc)

install(
  TARGETS ${DETECTOR_LIB}
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib/static)
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/lib${DETECTOR_LIB}_rdict.pcm
    ${CMAKE_CURRENT_BINARY_DIR}/lib${DETECTOR_LIB}.rootmap
    DESTINATION lib
    OPTIONAL)
install(FILES ${DETECTOR_HEADERS} DESTINATION include)
