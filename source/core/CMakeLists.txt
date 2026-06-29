# ---------------------------------------------------------------------------
# source/core  –  RestCore (shared library) Contains TRestEvent, TRestMetadata,
# TRestRun, TRestManager, TRestHits. ROOT dictionaries are generated with
# ROOT_GENERATE_DICTIONARY for classes that are stored in ROOT files
# (TRestEvent, TRestRun, TRestHits).
# ---------------------------------------------------------------------------

file(GLOB_RECURSE CORE_SOURCES src/*.cxx)
file(GLOB_RECURSE CORE_HEADERS inc/*.h)

add_library(RestCore SHARED ${CORE_SOURCES})

target_include_directories(
  RestCore PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
                  $<INSTALL_INTERFACE:include>)

target_link_libraries(RestCore PUBLIC RestTools ${ROOT_TARGET_LIBRARIES})

install(
  TARGETS RestCore
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib/static)
install(FILES ${CORE_HEADERS} DESTINATION include)
