# ---------------------------------------------------------------------------
# FindCRY - CRY (Cosmic-ray Shower Library, LLNL) Searched in CRY_PATH, $CRYHOME
# (set by the CRY setup script: source <cry>/setup) and $CRY_HOME. Defines
# CRY_FOUND, CRY_INCLUDE_DIR, CRY_LIBRARY, CRY_DATA_DIR and the imported target
# CRY::CRY. CRY builds only a static library (lib/libCRY.a), with the headers in
# src/ and the tables in data/.
# ---------------------------------------------------------------------------

set(_CRY_HINTS ${CRY_PATH} $ENV{CRYHOME} $ENV{CRY_HOME})

find_path(
  CRY_INCLUDE_DIR CRYGenerator.h
  HINTS ${_CRY_HINTS}
  PATH_SUFFIXES src include)
find_library(
  CRY_LIBRARY
  NAMES CRY
  HINTS ${_CRY_HINTS}
  PATH_SUFFIXES lib)
find_path(
  CRY_DATA_DIR cosmics_0.data
  HINTS $ENV{CRYDATAPATH} ${_CRY_HINTS}
  PATH_SUFFIXES data)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CRY REQUIRED_VARS CRY_LIBRARY CRY_INCLUDE_DIR
                                                    CRY_DATA_DIR)

if(CRY_FOUND AND NOT TARGET CRY::CRY)
  add_library(CRY::CRY UNKNOWN IMPORTED)
  set_target_properties(
    CRY::CRY PROPERTIES IMPORTED_LOCATION "${CRY_LIBRARY}"
                        INTERFACE_INCLUDE_DIRECTORIES "${CRY_INCLUDE_DIR}")
endif()

mark_as_advanced(CRY_INCLUDE_DIR CRY_LIBRARY CRY_DATA_DIR)
