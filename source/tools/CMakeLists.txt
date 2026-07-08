# ---------------------------------------------------------------------------
# source/tools  -  RestTools (shared library)
# ---------------------------------------------------------------------------

file(GLOB_RECURSE TOOLS_SOURCES src/*.cxx)
file(GLOB_RECURSE TOOLS_HEADERS inc/*.h)

add_library(RestTools SHARED ${TOOLS_SOURCES})

target_include_directories(
  RestTools PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
                   $<INSTALL_INTERFACE:include>)

target_link_libraries(RestTools PUBLIC yaml-cpp::yaml-cpp ${ROOT_TARGET_LIBRARIES})

rest_attach_dictionary(RestTools
  HEADERS ${TOOLS_HEADERS}
  INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/source/core/inc
    ${CMAKE_SOURCE_DIR}/libraries/raw/inc
    ${CMAKE_SOURCE_DIR}/libraries/detector/inc)

install(
  TARGETS RestTools
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib/static)
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/libRestTools_rdict.pcm
    ${CMAKE_CURRENT_BINARY_DIR}/libRestTools.rootmap
    DESTINATION lib
    OPTIONAL)
install(FILES ${TOOLS_HEADERS} DESTINATION include)
