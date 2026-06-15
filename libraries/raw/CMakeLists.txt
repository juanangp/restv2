# ---------------------------------------------------------------------------
# libraries/raw  –  RestRaw (shared library)
#   Contains TRestRawSignal, TRestRawSignalEvent.
#   Depends on RestCore (and transitively RestTools).
# ---------------------------------------------------------------------------

file(GLOB_RECURSE RAW_SOURCES src/*.cxx)
file(GLOB_RECURSE RAW_HEADERS inc/*.h)

add_library(RestRaw SHARED ${RAW_SOURCES})

target_include_directories(RestRaw
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(RestRaw
    PUBLIC
        RestCore
        RestTools
)

install(TARGETS RestRaw
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
)
install(FILES ${RAW_HEADERS} DESTINATION include)
