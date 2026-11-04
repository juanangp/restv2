# ---------------------------------------------------------------------------
# packages/restG4/test - the examples as tests (BUILD_TESTING): each test runs
# restG4 (and restManager) on its own copy of examples/ with few events and
# checks the output with the REST classes
# ---------------------------------------------------------------------------

add_executable(restG4Examples src/examples.cxx)
target_link_libraries(
  restG4Examples PRIVATE GTest::gtest_main RestCore RestTools ${GEANT4_LIB}
                         ${ROOT_TARGET_LIBRARIES})
if(COMMAND rest_link_registered_libraries)
  rest_link_registered_libraries(restG4Examples ${GEANT4_LIB})
endif()
target_compile_definitions(
  restG4Examples
  PRIVATE EXAMPLES_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../examples"
          TESTS_WORK_DIR="${CMAKE_CURRENT_BINARY_DIR}/work"
          RESTG4_EXECUTABLE="$<TARGET_FILE:restG4>"
          RESTMANAGER_EXECUTABLE="$<TARGET_FILE:restManager>")
add_dependencies(restG4Examples restG4 restManager)

include(GoogleTest)
# REST_HOME: runNumber "auto" ($REST_HOME/.rest/runNumber) and downloads, inside
# the build
gtest_discover_tests(
  restG4Examples DISCOVERY_TIMEOUT 60
  PROPERTIES ENVIRONMENT "REST_HOME=${CMAKE_CURRENT_BINARY_DIR}/work"
  TIMEOUT 3600 LABELS restG4)
