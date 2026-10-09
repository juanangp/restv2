# ---------------------------------------------------------------------------
# source/tools  -  RestAnalysis (shared library)
# ---------------------------------------------------------------------------

file(GLOB_RECURSE ANALYSIS_SOURCES src/*.cxx)
file(GLOB_RECURSE ANALYSIS_HEADERS inc/*.h)

add_library(RestAnalysis SHARED ${ANALYSIS_SOURCES})

target_include_directories(
  RestAnalysis PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
                   $<INSTALL_INTERFACE:include>)

target_link_libraries(RestAnalysis PUBLIC RestTools ${ROOT_TARGET_LIBRARIES})

rest_attach_dictionary(RestAnalysis
  HEADERS ${ANALYSIS_HEADERS}
  INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/source/tools/inc)

install(
  TARGETS RestAnalysis
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib/static)
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/libRestAnalysis_rdict.pcm
    ${CMAKE_CURRENT_BINARY_DIR}/libRestAnalysis.rootmap
    DESTINATION lib
    OPTIONAL)
install(FILES ${ANALYSIS_HEADERS} DESTINATION include)
