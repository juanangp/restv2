# ---------------------------------------------------------------------------
# source/bin  –  Executables Each .cxx file here becomes a separate executable
# linked against RestCore (and optionally RestRaw if the target is built).
# ---------------------------------------------------------------------------

file(GLOB BIN_SOURCES *.cxx)

foreach(src ${BIN_SOURCES})
  get_filename_component(exe_name ${src} NAME_WE)

  add_executable(${exe_name} ${src})

  target_link_libraries(${exe_name} PRIVATE RestCore
                                            $<$<TARGET_EXISTS:RestRaw>:RestRaw>)

  install(TARGETS ${exe_name} RUNTIME DESTINATION bin)
endforeach()
