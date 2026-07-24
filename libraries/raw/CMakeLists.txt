# ---------------------------------------------------------------------------
# libraries/raw  -  RestRaw (shared library)
# ---------------------------------------------------------------------------

file(GLOB_RECURSE RAW_SOURCES src/*.cxx)
file(GLOB_RECURSE RAW_HEADERS inc/*.h)

add_library(${RAW_LIB} SHARED ${RAW_SOURCES})

target_include_directories(
  ${RAW_LIB} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
                 $<INSTALL_INTERFACE:include>)

target_link_libraries(${RAW_LIB} PUBLIC RestCore RestTools ${ROOT_TARGET_LIBRARIES})

rest_attach_dictionary(${RAW_LIB}
  HEADERS ${RAW_HEADERS}
  INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/source/core/inc
    ${CMAKE_SOURCE_DIR}/source/tools/inc
    ${CMAKE_SOURCE_DIR}/libraries/detector/inc)

install(
  TARGETS ${RAW_LIB}
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib/static)
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/lib${RAW_LIB}_rdict.pcm
    ${CMAKE_CURRENT_BINARY_DIR}/lib${RAW_LIB}.rootmap
    DESTINATION lib
    OPTIONAL)
install(FILES ${RAW_HEADERS} DESTINATION include)
