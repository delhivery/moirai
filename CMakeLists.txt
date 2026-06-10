cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

project(moirai)
include(CTest)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_GENERATOR Ninja)
# ASAN set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}
# -fno-omit-frame-pointer -fsanitize=address")

# TSAN set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}
# -fno-omit-frame-pointer -Wthread-safety -fsanitize=thread")

# MSAN set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}
# -fno-omit-frame-pointer -fsanitize=memory")

# UBSAN set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}
# -fno-omit-frame-pointer -fsanitize=undefined") set(CMAKE_GCC_FLAGS
# "${CMAKE_CXX_FLAGS} -floop-nest-optimize")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Werror")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -fwhole-program -flto=auto -fgraphite-identity -floop-nest-optimize -fno-semantic-interposition -fno-plt -fuse-ld=mold")

    # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=mold")
    # set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}
    # -fuse-ld=mold") set(CMAKE_STATIC_LINKER_FLAGS
    # "${CMAKE_STATIC_LINKER_FLAGS} -fuse-ld=mold")
    # set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS}
    # -fuse-ld=mold")
    message(STATUS "Compiling with gcc")
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # set(CMAKE_CXX_FLAGS "-flto")

    # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=mold")
    # set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}
    # -fuse-ld=mold") set(CMAKE_STATIC_LINKER_FLAGS
    # "${CMAKE_STATIC_LINKER_FLAGS} -fuse-ld=mold")
    # set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS}
    # -fuse-ld=mold")
    message(STATUS "Compiling with clang")
  else()
    message(FATAL "Unsupported compiler. Supported compilers are gcc and clang.
    Found ${CMAKE_CXX_COMPILER_ID}")
  endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

option(ANALYZE "Analyze codebase for improvements" OFF)
option(LOCAL "Run EP locally with production dataset" OFF)
option(MOIRAI_BUILD_APP "Build the moirai application target" ON)

if(ANALYZE)
  message(STATUS "Analysis on")
  find_program(IWYU_PATH NAMES include-what-you-use iwyu REQUIRED)
  find_program(TIDY_PATH NAMES clang-tidy REQUIRED)
endif()

if(MOIRAI_BUILD_APP OR BUILD_TESTING)
  find_package(Boost COMPONENTS graph)
endif()

if(MOIRAI_BUILD_APP)
  find_package(TBB REQUIRED)
  find_package(CURL REQUIRED)
  find_package(OpenSSL REQUIRED)
  find_package(PkgConfig REQUIRED)
  find_package(RdKafkaCPP QUIET)

  if(RdKafkaCPP_FOUND)
    set(MOIRAI_RDKAFKA_TARGET RdKafkaCPP::RdKafkaCPP)
  else()
    pkg_check_modules(RDKAFKA REQUIRED IMPORTED_TARGET rdkafka++)
    add_library(RdKafkaCPP::RdKafkaCPP ALIAS PkgConfig::RDKAFKA)
    set(MOIRAI_RDKAFKA_TARGET RdKafkaCPP::RdKafkaCPP)
  endif()

  add_executable(
    ${PROJECT_NAME}
    src/date_utils.cxx
    src/graph_helpers.cxx
    src/transportation.cxx
    src/solver.cxx
    src/utils.cxx
    src/scan_reader.cxx
    src/kafka_reader.cxx
    src/search_writer.cxx
    src/file_reader.cxx
    src/processor.cxx
    src/http.cxx
    src/solver_wrapper.cxx
    src/server.cxx)
  target_include_directories(${PROJECT_NAME} PUBLIC include)

  if(ANALYZE)
    set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_INCLUDE_WHAT_YOU_USE
                                                 ${IWYU_PATH})
    set(CMAKE_CXX_CLANG_TIDY clang-tidy -checks=-*,readability-*)
  endif()

  target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE Boost::graph
            CURL::libcurl
            ${MOIRAI_RDKAFKA_TARGET}
            TBB::tbb
            OpenSSL::SSL
            OpenSSL::Crypto)
endif()

if(BUILD_TESTING)
  find_package(CURL REQUIRED)
  find_package(OpenSSL REQUIRED)

  function(moirai_configure_test target)
    target_include_directories(${target} PRIVATE include tests)
    target_compile_definitions(
      ${target}
      PRIVATE MOIRAI_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME ${target} COMMAND ${target})
  endfunction()

  add_executable(moirai_schedule_tests tests/schedule_tests.cxx
                                        src/date_utils.cxx
                                        src/transportation.cxx)
  moirai_configure_test(moirai_schedule_tests)

  add_executable(
    moirai_solver_tests
    tests/solver_tests.cxx
    src/date_utils.cxx
    src/graph_helpers.cxx
    src/solver.cxx
    src/transportation.cxx)
  moirai_configure_test(moirai_solver_tests)
  target_link_libraries(moirai_solver_tests PRIVATE Boost::graph)

  add_executable(moirai_processor_tests tests/processor_tests.cxx
                                        src/date_utils.cxx
                                        src/processor.cxx
                                        src/transportation.cxx)
  moirai_configure_test(moirai_processor_tests)

  add_executable(moirai_logger_tests tests/logger_tests.cxx)
  moirai_configure_test(moirai_logger_tests)

  set(MOIRAI_WRAPPER_TEST_SOURCES
      src/date_utils.cxx
      src/graph_helpers.cxx
      src/http.cxx
      src/processor.cxx
      src/solver.cxx
      src/solver_wrapper.cxx
      src/transportation.cxx
      src/utils.cxx)

  add_executable(
    moirai_wrapper_tests
    tests/wrapper_tests.cxx
    ${MOIRAI_WRAPPER_TEST_SOURCES})
  moirai_configure_test(moirai_wrapper_tests)
  target_link_libraries(moirai_wrapper_tests PRIVATE Boost::graph
                                                     CURL::libcurl
                                                     OpenSSL::Crypto)

  add_executable(
    moirai_init_tests
    tests/init_tests.cxx
    ${MOIRAI_WRAPPER_TEST_SOURCES})
  moirai_configure_test(moirai_init_tests)
  target_link_libraries(moirai_init_tests PRIVATE Boost::graph
                                                  CURL::libcurl
                                                  OpenSSL::Crypto)

  add_executable(
    moirai_run_tests
    tests/run_tests.cxx
    ${MOIRAI_WRAPPER_TEST_SOURCES})
  moirai_configure_test(moirai_run_tests)
  target_link_libraries(moirai_run_tests PRIVATE Boost::graph
                                                 CURL::libcurl
                                                 OpenSSL::Crypto)
endif()
