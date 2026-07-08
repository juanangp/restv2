# ---------------------------------------------------------------------------
# source/core  -  RestCore (shared library)
# ---------------------------------------------------------------------------

file(GLOB_RECURSE CORE_SOURCES src/*.cxx)
file(GLOB_RECURSE CORE_HEADERS inc/*.h)

add_library(RestCore SHARED ${CORE_SOURCES})

target_include_directories(
  RestCore PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
                  $<INSTALL_INTERFACE:include>)

target_link_libraries(RestCore PUBLIC RestTools ${ROOT_TARGET_LIBRARIES})

rest_attach_dictionary(RestCore
  HEADERS ${CORE_HEADERS}
  INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/source/tools/inc
    ${CMAKE_SOURCE_DIR}/libraries/raw/inc
    ${CMAKE_SOURCE_DIR}/libraries/detector/inc)

install(
  TARGETS RestCore
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib/static)
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/libRestCore_rdict.pcm
    ${CMAKE_CURRENT_BINARY_DIR}/libRestCore.rootmap
    DESTINATION lib
    OPTIONAL)
install(FILES ${CORE_HEADERS} DESTINATION include)
