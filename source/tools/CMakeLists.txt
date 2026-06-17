# ---------------------------------------------------------------------------
# source/tools  –  RestTools (shared library) No ROOT dictionary needed: these
# are pure C++/yaml-cpp helpers.
# ---------------------------------------------------------------------------

file(GLOB_RECURSE TOOLS_SOURCES src/*.cxx)
file(GLOB_RECURSE TOOLS_HEADERS inc/*.h)

add_library(RestTools SHARED ${TOOLS_SOURCES})

target_include_directories(
  RestTools PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
                   $<INSTALL_INTERFACE:include>)

target_link_libraries(RestTools PUBLIC yaml-cpp::yaml-cpp)

install(
  TARGETS RestTools
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib/static)
install(FILES ${TOOLS_HEADERS} DESTINATION include)
