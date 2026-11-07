# ---------------------------------------------------------------------------
# packages/restDAQ - optional data taking application for AFTER/AGET electronics
# (ARC, FEMINOS, DCC and TCM), formerly FEMDAQ; executable femclient. It writes
# REST run files with TRestRawDAQSignalEvent (raw library). Requires readline
# and prometheus-cpp (found with find_package, e.g.
# CMAKE_PREFIX_PATH=<prometheus install>, otherwise downloaded with
# FetchContent).
# ---------------------------------------------------------------------------

find_package(Threads REQUIRED)

find_path(READLINE_INCLUDE_DIR NAMES readline/readline.h)
find_library(READLINE_LIBRARY NAMES readline)
if(NOT READLINE_INCLUDE_DIR OR NOT READLINE_LIBRARY)
  message(FATAL_ERROR "restDAQ: readline not found (e.g. libreadline-dev)")
endif()

find_package(prometheus-cpp CONFIG QUIET)
if(NOT prometheus-cpp_FOUND)
  message(STATUS "restDAQ: prometheus-cpp not found, downloading it")
  include(FetchContent)
  set(ENABLE_TESTING
      OFF
      CACHE BOOL "" FORCE)
  set(ENABLE_PUSH
      OFF
      CACHE BOOL "" FORCE)
  set(THIRDPARTY_FORCE_PIC
      ON
      CACHE BOOL "" FORCE)
  FetchContent_Declare(
    prometheus-cpp
    GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp.git
    GIT_TAG v1.2.4)
  FetchContent_MakeAvailable(prometheus-cpp)
endif()

file(GLOB_RECURSE RESTDAQ_SOURCES src/*.cxx)

add_executable(femclient main.cxx ${RESTDAQ_SOURCES})
target_include_directories(femclient PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
                                             ${READLINE_INCLUDE_DIR})
target_link_libraries(
  femclient
  PRIVATE RestCore
          RestTools
          ${RAW_LIB}
          prometheus-cpp::core
          prometheus-cpp::pull
          ${READLINE_LIBRARY}
          Threads::Threads
          ${ROOT_TARGET_LIBRARIES}
          yaml-cpp::yaml-cpp)

install(TARGETS femclient RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/examples/
        DESTINATION examples/restDAQ)
