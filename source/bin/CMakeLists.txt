# ---------------------------------------------------------------------------
# source/bin  –  Executables Each .cxx file here becomes a separate executable
# linked against RestCore and every REST library that is built.
# ---------------------------------------------------------------------------

file(GLOB BIN_SOURCES *.cxx)

foreach(src ${BIN_SOURCES})
  get_filename_component(exe_name ${src} NAME_WE)

  add_executable(${exe_name} ${src})

  # Every REST library, so that all the registered events and processes are
  # available
  rest_link_registered_libraries(${exe_name} RestCore ${REST_LIBRARIES})

  # Link all configured ROOT libraries in every executable.
  if(ROOT_TARGET_LIBRARIES)
    target_link_libraries(${exe_name} PRIVATE ${ROOT_TARGET_LIBRARIES})
  endif()

  # Keep Rint available when present (used by restRoot interactive entrypoint).
  if(TARGET ROOT::Rint)
    target_link_libraries(${exe_name} PRIVATE ROOT::Rint)
  endif()

  install(TARGETS ${exe_name} RUNTIME DESTINATION bin)
endforeach()
