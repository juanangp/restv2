# ---------------------------------------------------------------------------
# libraries/track  -  RestTrack (shared library)
# ---------------------------------------------------------------------------

file(GLOB_RECURSE TRACK_SOURCES src/*.cxx)
file(GLOB_RECURSE TRACK_HEADERS inc/*.h)

add_library(${TRACK_LIB} SHARED ${TRACK_SOURCES})

target_include_directories(
  ${TRACK_LIB} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
                      $<INSTALL_INTERFACE:include>)

target_link_libraries(${TRACK_LIB} PUBLIC RestCore RestTools ${ROOT_TARGET_LIBRARIES})

rest_attach_dictionary(${TRACK_LIB}
  HEADERS ${TRACK_HEADERS}
  INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/source/core/inc
    ${CMAKE_SOURCE_DIR}/source/tools/inc)

install(
  TARGETS ${TRACK_LIB}
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib/static)
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/lib${TRACK_LIB}_rdict.pcm
    ${CMAKE_CURRENT_BINARY_DIR}/lib${TRACK_LIB}.rootmap
    DESTINATION lib
    OPTIONAL)
install(FILES ${TRACK_HEADERS} DESTINATION include)
