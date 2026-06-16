cmake_minimum_required(VERSION 4.3)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
    "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS
    ON
    CACHE BOOL "Suppress CMake developer warnings")
if(NOT DEFINED CMAKE_CXX_STANDARD_LIBRARY)
  set(CMAKE_CXX_STANDARD_LIBRARY
      "libstdc++"
      CACHE STRING "C++ standard library used for import std")
endif()
set(MOIRAI_CXX_STANDARD_LIBRARY "${CMAKE_CXX_STANDARD_LIBRARY}")

set(MOIRAI_BOOTSTRAP_CXX_COMPILER "${CMAKE_CXX_COMPILER}")
if(NOT MOIRAI_BOOTSTRAP_CXX_COMPILER AND DEFINED ENV{CXX})
  set(MOIRAI_BOOTSTRAP_CXX_COMPILER "$ENV{CXX}")
endif()

if(MOIRAI_BOOTSTRAP_CXX_COMPILER MATCHES "clang(\\+\\+)?$" AND
   MOIRAI_CXX_STANDARD_LIBRARY STREQUAL "libstdc++" AND
   NOT DEFINED CMAKE_CXX_STDLIB_MODULES_JSON)
  execute_process(
    COMMAND ${MOIRAI_BOOTSTRAP_CXX_COMPILER} -stdlib=libstdc++
            -print-file-name=libstdc++.modules.json
    OUTPUT_VARIABLE MOIRAI_CLANG_LIBSTDCXX_MODULES_JSON
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(CMAKE_CXX_STDLIB_MODULES_JSON
      "${MOIRAI_CLANG_LIBSTDCXX_MODULES_JSON}"
      CACHE FILEPATH "C++ standard library module metadata")
endif()
if(MOIRAI_BOOTSTRAP_CXX_COMPILER MATCHES "clang(\\+\\+)?$")
  string(APPEND CMAKE_CXX_FLAGS_INIT
         " -stdlib=${MOIRAI_CXX_STANDARD_LIBRARY}")
endif()

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

project(moirai LANGUAGES C CXX)
include(CTest)

if(NOT CMAKE_GENERATOR MATCHES "Ninja")
  message(FATAL_ERROR "C++ modules/import std require a Ninja generator")
endif()

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  message(FATAL_ERROR "Unsupported compiler ${CMAKE_CXX_COMPILER_ID}; use GCC or Clang")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15)
  message(FATAL_ERROR "GCC 15 or newer is required for import std")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 18.1.2)
  message(FATAL_ERROR "Clang 18.1.2 or newer is required for import std")
endif()

if(NOT 26 IN_LIST CMAKE_CXX_COMPILER_IMPORT_STD)
  message(FATAL_ERROR
          "The active ${CMAKE_CXX_COMPILER_ID} toolchain does not support import std for C++26: "
          "${CMAKE_CXX_COMPILER_IMPORT_STD_ERROR_MESSAGE}")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
   MOIRAI_CXX_STANDARD_LIBRARY STREQUAL "libc++")
  add_link_options(-stdlib=libc++)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
       MOIRAI_CXX_STANDARD_LIBRARY STREQUAL "libstdc++")
  add_link_options(-stdlib=libstdc++)
endif()

# ASAN set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}
# -fno-omit-frame-pointer -fsanitize=address")

# TSAN set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}
# -fno-omit-frame-pointer -Wthread-safety -fsanitize=thread")

# MSAN set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}
# -fsanitize=memory")

# UBSAN set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}
# -fno-omit-frame-pointer -fsanitize=undefined")

add_compile_options(
  -O3
  "$<$<CXX_COMPILER_ID:Clang>:-Wno-reserved-module-identifier>"
  "$<$<CXX_COMPILER_ID:Clang>:-D_FORTIFY_SOURCE=0>")

set(MOIRAI_PROJECT_WARNINGS -Wall -Wextra -Wpedantic -Werror)
set(MOIRAI_GNU_WARNINGS -Wno-maybe-uninitialized
                        -Wno-expose-global-module-tu-local)
set(MOIRAI_CLANG_WARNINGS -Wno-reserved-module-identifier)

set(MOIRAI_PGO_MODE
    ""
    CACHE STRING "PGO mode: empty, generate, or use")
set(MOIRAI_PGO_DIR
    "${CMAKE_BINARY_DIR}/pgo"
    CACHE PATH "Profile data directory for MOIRAI_PGO_MODE")
set(MOIRAI_SOLVER_QUEUE
    "bucket"
    CACHE STRING "Solver queue implementation: binary or bucket")
option(MOIRAI_ENABLE_GCC_LTO
       "Enable GCC LTO for Release builds; disabled by default while GCC modules support is unstable"
       OFF)

if(NOT MOIRAI_PGO_MODE STREQUAL "generate" AND
   NOT MOIRAI_PGO_MODE STREQUAL "use" AND
   NOT MOIRAI_PGO_MODE STREQUAL "")
  message(FATAL_ERROR "MOIRAI_PGO_MODE must be empty, generate, or use")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(MOIRAI_ENABLE_GCC_LTO AND MOIRAI_PGO_MODE STREQUAL "")
      add_compile_options(-fwhole-program -flto=auto)
    elseif(MOIRAI_ENABLE_GCC_LTO)
      message(STATUS "Disabling gcc LTO/whole-program flags for PGO module build")
    else()
      message(STATUS "GCC LTO disabled for module build")
    endif()
    add_compile_options(-fgraphite-identity -floop-nest-optimize
                        -fno-semantic-interposition -fno-plt)
    add_link_options(-fuse-ld=mold)
    message(STATUS "Compiling with gcc")
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "Compiling with clang")
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

if(MOIRAI_PGO_MODE STREQUAL "generate")
  file(MAKE_DIRECTORY "${MOIRAI_PGO_DIR}")
  add_compile_options(-fprofile-generate=${MOIRAI_PGO_DIR})
  add_link_options(-fprofile-generate=${MOIRAI_PGO_DIR})
elseif(MOIRAI_PGO_MODE STREQUAL "use")
  add_compile_options(-fprofile-use=${MOIRAI_PGO_DIR}
                      "$<$<CXX_COMPILER_ID:GNU>:-Wno-missing-profile>"
                      "$<$<CXX_COMPILER_ID:Clang>:-Wno-profile-instr-missing>")
  add_link_options(-fprofile-use=${MOIRAI_PGO_DIR})
elseif(NOT MOIRAI_PGO_MODE STREQUAL "")
  message(FATAL_ERROR "MOIRAI_PGO_MODE must be empty, generate, or use")
endif()

if(NOT MOIRAI_SOLVER_QUEUE STREQUAL "binary" AND
   NOT MOIRAI_SOLVER_QUEUE STREQUAL "bucket")
  message(FATAL_ERROR "MOIRAI_SOLVER_QUEUE must be binary or bucket")
endif()

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(TBB REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(RdKafkaCPP QUIET)

if(RdKafkaCPP_FOUND)
  set(MOIRAI_RDKAFKA_TARGET RdKafkaCPP::RdKafkaCPP)
else()
  pkg_check_modules(RDKAFKA REQUIRED IMPORTED_TARGET rdkafka++)
  add_library(RdKafkaCPP::RdKafkaCPP ALIAS PkgConfig::RDKAFKA)
  set(MOIRAI_RDKAFKA_TARGET RdKafkaCPP::RdKafkaCPP)
endif()

set(MOIRAI_MODULE_SOURCES
    modules/moirai.date_utils.cxx
    modules/moirai.json_utils.cxx
    modules/moirai.transportation.cxx
    modules/moirai.route_schedule.cxx
    modules/moirai.solver.cxx
    modules/moirai.search_document.cxx
    modules/moirai.processor.cxx
    modules/moirai.app.cxx
    modules/moirai.utils.cxx
    modules/moirai.http.cxx
    modules/moirai.scan_reader.cxx
    modules/moirai.file_reader.cxx
    modules/moirai.kafka_reader.cxx
    modules/moirai.search_writer.cxx
    modules/moirai.solver_wrapper.cxx
    modules/moirai.server.cxx)

set(MOIRAI_IMPLEMENTATION_SOURCES
    src/date_utils.cxx
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

add_library(moirai_core STATIC ${MOIRAI_IMPLEMENTATION_SOURCES})
target_sources(moirai_core
  PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/modules"
    FILES ${MOIRAI_MODULE_SOURCES})
target_include_directories(moirai_core PUBLIC include)
target_compile_features(moirai_core PUBLIC cxx_std_26)
set_property(TARGET moirai_core PROPERTY CXX_MODULE_STD ON)
target_compile_options(
  moirai_core
  PRIVATE ${MOIRAI_PROJECT_WARNINGS}
          "$<$<CXX_COMPILER_ID:GNU>:${MOIRAI_GNU_WARNINGS}>"
          "$<$<CXX_COMPILER_ID:Clang>:${MOIRAI_CLANG_WARNINGS}>")
if(MOIRAI_SOLVER_QUEUE STREQUAL "bucket")
  target_compile_definitions(moirai_core PRIVATE MOIRAI_SOLVER_QUEUE_BUCKET=1)
endif()
target_link_libraries(
  moirai_core
  PUBLIC CURL::libcurl
         ${MOIRAI_RDKAFKA_TARGET}
         TBB::tbb
         ZLIB::ZLIB
         OpenSSL::SSL
         OpenSSL::Crypto)

if(ANALYZE)
  set_property(TARGET moirai_core PROPERTY CXX_INCLUDE_WHAT_YOU_USE
                                           ${IWYU_PATH})
  set(CMAKE_CXX_CLANG_TIDY clang-tidy -checks=-*,readability-*)
endif()

function(moirai_configure_binary target)
  target_include_directories(${target} PRIVATE include tests)
  target_compile_features(${target} PRIVATE cxx_std_26)
  set_property(TARGET ${target} PROPERTY CXX_MODULE_STD ON)
  target_compile_options(
    ${target}
    PRIVATE ${MOIRAI_PROJECT_WARNINGS}
            "$<$<CXX_COMPILER_ID:GNU>:${MOIRAI_GNU_WARNINGS}>"
            "$<$<CXX_COMPILER_ID:Clang>:${MOIRAI_CLANG_WARNINGS}>")
  target_link_libraries(${target} PRIVATE moirai_core)
endfunction()

if(MOIRAI_BUILD_APP)
  add_executable(${PROJECT_NAME} src/main.cxx)
  moirai_configure_binary(${PROJECT_NAME})
endif()

if(BUILD_TESTING)
  function(moirai_configure_test target)
    moirai_configure_binary(${target})
    target_compile_definitions(
      ${target}
      PRIVATE MOIRAI_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME ${target} COMMAND ${target})
  endfunction()

  add_executable(moirai_schedule_tests tests/schedule_tests.cxx)
  moirai_configure_test(moirai_schedule_tests)

  add_executable(moirai_solver_tests tests/solver_tests.cxx)
  moirai_configure_test(moirai_solver_tests)

  add_executable(moirai_processor_tests tests/processor_tests.cxx)
  moirai_configure_test(moirai_processor_tests)

  add_executable(moirai_logger_tests tests/logger_tests.cxx)
  moirai_configure_test(moirai_logger_tests)

  add_executable(moirai_wrapper_tests tests/wrapper_tests.cxx)
  moirai_configure_test(moirai_wrapper_tests)

  add_executable(moirai_init_tests tests/init_tests.cxx)
  moirai_configure_test(moirai_init_tests)

  add_executable(moirai_run_tests tests/run_tests.cxx)
  moirai_configure_test(moirai_run_tests)

  add_executable(moirai_search_writer_tests tests/search_writer_tests.cxx)
  moirai_configure_test(moirai_search_writer_tests)

  add_executable(moirai_module_smoke tests/module_smoke.cxx)
  moirai_configure_test(moirai_module_smoke)

  add_executable(moirai_solver_benchmarks benchmarks/solver_benchmarks.cxx)
  moirai_configure_test(moirai_solver_benchmarks)
  add_custom_target(moirai_pgo_train
                    COMMAND $<TARGET_FILE:moirai_solver_benchmarks>
                    DEPENDS moirai_solver_benchmarks
                    COMMENT "Running solver benchmark PGO training workload")

  if(TARGET moirai)
    add_test(NAME moirai_cli_help COMMAND $<TARGET_FILE:moirai> --help)
    set_tests_properties(moirai_cli_help PROPERTIES
                         PASS_REGULAR_EXPRESSION "Usage: moirai")

    add_test(NAME moirai_cli_missing_required
             COMMAND bash -c "\"$1\"; code=$?; test $code -eq 1" _
                     $<TARGET_FILE:moirai>)
    set_tests_properties(
      moirai_cli_missing_required
      PROPERTIES PASS_REGULAR_EXPRESSION "Missing required option --route-api")

    add_test(NAME moirai_cli_invalid_kafka_config
             COMMAND bash -c
                     "\"$1\" --kafka-config invalid; code=$?; test $code -eq 1"
                     _ $<TARGET_FILE:moirai>)
    set_tests_properties(
      moirai_cli_invalid_kafka_config
      PROPERTIES PASS_REGULAR_EXPRESSION "Invalid --kafka-config")

    add_test(
      NAME moirai_cli_local_mode_skips_kafka_requirements
      COMMAND
        bash -c
        "\"$1\" --query-from \"$2\" --route-api http://route.test/routes --facility-timings \"$3\" --facility-api http://facility.test/facilities --route-token route-token --facility-token facility-token --search-index moirai --search-uri http://search.test --search-user user; code=$?; test $code -eq 1"
        _
        $<TARGET_FILE:moirai>
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/load_normal.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/timings.json")
    set_tests_properties(
      moirai_cli_local_mode_skips_kafka_requirements
      PROPERTIES PASS_REGULAR_EXPRESSION "Missing required option --search-pass")

    add_test(
      NAME moirai_cli_numeric_options_parse
      COMMAND
        bash -c
        "\"$1\" --batch-timeout 250 --batch-size 64 --route-api http://route.test/routes --facility-timings \"$2\" --facility-api http://facility.test/facilities --route-token route-token --facility-token facility-token --search-index moirai --search-uri http://search.test --search-user user; code=$?; test $code -eq 1"
        _
        $<TARGET_FILE:moirai>
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/timings.json")
    set_tests_properties(
      moirai_cli_numeric_options_parse
      PROPERTIES PASS_REGULAR_EXPRESSION "Missing required option --search-pass")
  endif()
endif()
