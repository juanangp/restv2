# ---------------------------------------------------------------------------
# libraries/detector  –  RestDetector (shared library)
# ---------------------------------------------------------------------------

file(GLOB_RECURSE DETECTOR_SOURCES src/*.cxx)
file(GLOB_RECURSE DETECTOR_HEADERS inc/*.h)

add_library(RestDetector SHARED ${DETECTOR_SOURCES})

target_include_directories(
  RestDetector PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
                 $<INSTALL_INTERFACE:include>)

target_link_libraries(RestDetector PUBLIC RestCore RestTools ${ROOT_TARGET_LIBRARIES})

install(
  TARGETS RestDetector
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib/static)
install(FILES ${DETECTOR_HEADERS} DESTINATION include)
