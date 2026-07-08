# ---------------------------------------------------------------------------
# libraries/detector  -  RestDetector (shared library)
# ---------------------------------------------------------------------------

file(GLOB_RECURSE DETECTOR_SOURCES src/*.cxx)
file(GLOB_RECURSE DETECTOR_HEADERS inc/*.h)

add_library(RestDetector SHARED ${DETECTOR_SOURCES})

target_include_directories(
  RestDetector PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
                      $<INSTALL_INTERFACE:include>)

target_link_libraries(RestDetector PUBLIC RestCore RestTools ${ROOT_TARGET_LIBRARIES})

rest_attach_dictionary(RestDetector
  HEADERS ${DETECTOR_HEADERS}
  INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/source/core/inc
    ${CMAKE_SOURCE_DIR}/source/tools/inc
    ${CMAKE_SOURCE_DIR}/libraries/raw/inc)

install(
  TARGETS RestDetector
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib/static)
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/libRestDetector_rdict.pcm
    ${CMAKE_CURRENT_BINARY_DIR}/libRestDetector.rootmap
    DESTINATION lib
    OPTIONAL)
install(FILES ${DETECTOR_HEADERS} DESTINATION include)
