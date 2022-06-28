cmake_minimum_required(VERSION 3.10)

project(moirai)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set(CMAKE_GENERATOR Ninja)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -fuse-ld=mold") Treat warnings
# as errors: Werror Stop compiling on first error: Wfatal-errors
include(CheckCXXCompilerFlag)

macro(CHECK_AND_ADD_CFLAG _FLAG)
  check_cxx_compiler_flag(${_FLAG} HAVE_FLAG)

  if(HAVE_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_FLAG}")
  endif()
endmacro()

check_and_add_cflag("-Werror")
check_and_add_cflag("-Wfatal-errors")

option(NODE_FILE "Load `nodes` from file" OFF)
option(EDGE_FILE "Load `edges` from file" OFF)
option(LOAD_FILE "Load `loads` from file" OFF)
option(SYNC_OFF "Disable sync to elasticsearch" OFF)
option(LINT "Verify codebase for styling errors" OFF)
option(COVERAGE "Enable coverage" OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(COVERAGE)
    include(CodeCoverage)
    check_and_add_cflag("-g")
    check_and_add_cflag("-O0")
    check_and_add_cflag("-Wall")
    check_and_add_cflag("-fprofile-arcs")
    check_and_add_cflag("-ftest-coverage")
    set(COVERAGE_EXCLUDES "test/*")
  else()
    set(SANITIZE
        "off"
        CACHE STRING "Enable sanitizer")
    set_property(CACHE SANITIZE PROPERTY STRINGS "off" "address" "thread"
                                         "memory" "undefined")
    get_property(
      SANITIZE_STRINGS
      CACHE SANITIZE
      PROPERTY STRINGS)

    if(NOT SANITIZE IN_LIST SANITIZE_STRINGS)
      message(FATAL_ERROR "Wrong value for sanitization")
    endif()

    if(NOT SANITIZE STREQUAL "off")
      set(CMAKE_CXX_FLAGS_DEBUG
          "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=${SANITIZE}"
      )
      if(SANITIZE STREQUAL "thread")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wthread-safety")
      endif()
    endif()
  endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

  check_and_add_cflag("-stdlib=libstdc++")
  check_and_add_cflag("-fwhole-program")
  check_and_add_cflag("-fgraphite-identity")
  check_and_add_cflag("-floop-nest-optimize")
  check_and_add_cflag("-fno-semantic-interposition")
  check_and_add_cflag("-fno-plt")
  check_and_add_cflag("-flto=auto")

  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
  else()
    message(FATAL_ERROR "Unsupported compiler")
  endif()
endif()

find_package(Boost COMPONENTS graph)
find_package(TBB REQUIRED)
find_package(fmt REQUIRED)
find_package(TBB REQUIRED)
# find_package(Poco REQUIRED Util Foundation Net NetSSL)
find_package(OpenSSL REQUIRED)
find_package(RdKafkaCPP REQUIRED)
find_package(Catch2 3 REQUIRED)
find_package(range-v3 REQUIRED)

set(CPR_FORCE_USE_SYSTEM_CURL ON)
set(CPR_FORCE_OPENSSL_BACKEND ON)

include(FetchContent)
FetchContent_Declare(
  cpr
  GIT_REPOSITORY https://github.com/libcpr/cpr.git
  GIT_TAG 3c45a497ae714d49c3b44d189424993ebb2085ed)
FetchContent_MakeAvailable(cpr)

set(RANGES_CXX_STD ON)
set(RANGES_BUILD_CALENDAR_EXAMPLE OFF)
set(RANGES_DEBUG_INFO OFF)
set(RANGES_DEEP_STL_INTEGRATION ON)
# Dependency

# utils ->

# date ->

# runnable -> concepts

# consumer -> runnable

# producer -> runnable

# edge -> concepts, date

# producer_api -> producer

# producer_faas -> producer_api

# producer_thanos -> producer_api

# producer_kafka -> producer

# producer_file -> producer

# consumer_search -> consumer, utils

# consumer_console -> consumer

# transport_center -> date, transport

# transport_edge -> edge, transport

# hash -> transport_edge, transport_center

# solver -> graph_helpers, hash, transport_edge, transport_center

add_library(
  ${PROJECT_NAME} src/producer_api.cxx src/producer.cxx src/consumer.cxx
                  src/runnable.cxx src/date.cxx src/utils.cxx)
# src/solver.cxx

# src/hash.cxx

# src/transport_edge.cxx

# src/transport_center.cxx

# src/consumer_console.cxx

# src/consumer_search.cxx

# src/producer_file.cxx

# src/producer_kafka.cxx

# src/edge.cxx

# src/producer_thanos.cxx

# src/producer_faas.cxx

add_executable(${PROJECT_NAME}tests tests/date.cxx tests/utils.cxx
                                    tests/base64.cxx)

# src/server.cxx)

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME}tests PUBLIC include)
# add_compile_definitions(JSON_HAS_CPP_20)
# add_compile_definitions(JSON_HAS_RANGES)

if(COVERAGE)
  setup_target_for_coverage_gcovr_html(NAME coverage EXECUTABLE
                                       ${PROJECT_NAME}tests)
endif()

if(NODE_FILE)
  add_compile_definitions(WITH_NODE_FILE)
endif()

if(EDGE_FILE)
  add_compile_definitions(WITH_EDGE_FILE)
endif()

if(LOAD_FILE)
  add_compile_definitions(WITH_LOAD_FILE)
endif()

if(NOT SYNC_OFF)
  add_compile_definitions(ENABLE_SYNC)
endif()

if(LINT)
  find_program(IWYU_PATH NAMES include-what-you-use iwyu REQUIRED)
  find_program(TIDY_PATH NAMES clang-tidy REQUIRED)

  set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_INCLUDE_WHAT_YOU_USE
                                               ${IWYU_PATH})
  set(CMAKE_CXX_CLANG_TIDY clang-tidy -checks=-*,readability-*)
endif()

include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME}tests)
target_link_libraries(
  ${PROJECT_NAME} PRIVATE cpr::cpr range-v3::range-v3 Boost::graph
                          RdKafkaCPP::RdKafkaCPP TBB::tbb fmt::fmt-header-only)
target_link_libraries(${PROJECT_NAME}tests PRIVATE Catch2::Catch2WithMain
                                                   ${PROJECT_NAME})
