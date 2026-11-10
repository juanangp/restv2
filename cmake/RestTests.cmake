# ---------------------------------------------------------------------------
# Unit tests (BUILD_TESTING, GoogleTest): rest_add_tests(<label> <library>...)
# in the test/ directory of a component builds the executable <label>Tests from
# test/src/*.cxx, linked with the REST libraries given (and those they need),
# and registers each GoogleTest test in ctest with the label of the component
# (ctest -L <label>). The tests run in test/work of the build directory
# (TESTS_WORK_DIR, also REST_HOME); TESTS_SOURCE_DIR is the test/ directory of
# the sources (files used by the tests). The executables are dependencies of the
# target check (build and run all the tests).
# ---------------------------------------------------------------------------
include(GoogleTest)

function(rest_add_tests label)
  file(GLOB _sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx)
  if(NOT _sources)
    return()
  endif()
  set(_target ${label}Tests)
  set(_work ${CMAKE_CURRENT_BINARY_DIR}/work)
  file(MAKE_DIRECTORY ${_work})
  add_executable(${_target} ${_sources})
  target_link_libraries(
    ${_target} PRIVATE GTest::gtest_main ${ROOT_TARGET_LIBRARIES}
                       yaml-cpp::yaml-cpp)
  rest_link_registered_libraries(${_target} ${ARGN})
  target_compile_definitions(
    ${_target} PRIVATE TESTS_WORK_DIR="${_work}"
                       TESTS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  gtest_discover_tests(
    ${_target} DISCOVERY_TIMEOUT 60
    WORKING_DIRECTORY ${_work}
    PROPERTIES ENVIRONMENT "REST_HOME=${_work}"
    TIMEOUT 600 LABELS ${label})
  add_dependencies(rest_tests ${_target})
endfunction()
