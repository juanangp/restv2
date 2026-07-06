# ---------------------------------------------------------------------------
# Generates thisREST.sh (and thisREST.csh) in the install prefix.
# Uses configure_file() so no escaping gymnastics are needed.
# ---------------------------------------------------------------------------

# Locate thisroot.sh for the ROOT version used at compile time
execute_process(
  COMMAND root-config --prefix
  OUTPUT_VARIABLE ROOT_PATH
  OUTPUT_STRIP_TRAILING_WHITESPACE)
set(thisROOT "${ROOT_PATH}/bin/thisroot.sh")

# Generate the scripts into the build tree; install() copies them
configure_file(
  "${CMAKE_CURRENT_LIST_DIR}/thisREST.sh.in"
  "${CMAKE_BINARY_DIR}/thisREST.sh"
  @ONLY)

configure_file(
  "${CMAKE_CURRENT_LIST_DIR}/thisREST.csh.in"
  "${CMAKE_BINARY_DIR}/thisREST.csh"
  @ONLY)

install(
  FILES
    "${CMAKE_BINARY_DIR}/thisREST.sh"
    "${CMAKE_BINARY_DIR}/thisREST.csh"
  DESTINATION "${CMAKE_INSTALL_PREFIX}"
  PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)
