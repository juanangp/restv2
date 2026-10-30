# ---------------------------------------------------------------------------
# Generates thisREST.sh (and thisREST.csh) in the install prefix. Uses
# configure_file() so no escaping gymnastics are needed.
# ---------------------------------------------------------------------------

# Locate thisroot.sh for the ROOT version used at compile time. ROOT_BINDIR
# comes from find_package(ROOT), so root-config does not need to be in PATH.
# Distribution packages (e.g. Fedora) may not ship thisroot.sh: the generated
# script only sources it if it exists.
set(thisROOT "${ROOT_BINDIR}/thisroot.sh")

# Generate the scripts into the build tree; install() copies them
configure_file("${CMAKE_CURRENT_LIST_DIR}/thisREST.sh.in"
               "${CMAKE_BINARY_DIR}/thisREST.sh" @ONLY)

configure_file("${CMAKE_CURRENT_LIST_DIR}/thisREST.csh.in"
               "${CMAKE_BINARY_DIR}/thisREST.csh" @ONLY)

install(
  FILES "${CMAKE_BINARY_DIR}/thisREST.sh" "${CMAKE_BINARY_DIR}/thisREST.csh"
  DESTINATION "${CMAKE_INSTALL_PREFIX}"
  PERMISSIONS
    OWNER_READ
    OWNER_WRITE
    OWNER_EXECUTE
    GROUP_READ
    GROUP_EXECUTE
    WORLD_READ
    WORLD_EXECUTE)
