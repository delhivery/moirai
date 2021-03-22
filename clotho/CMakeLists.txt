cmake_minimum_required(VERSION 3.10)

project(clotho)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_GENERATOR Ninja)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -fno-omit-frame-pointer -ggdb")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -fno-omit-frame-pointer -ggdb")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_BUILD_TYPE)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

option(ANALYZE "Analyze codebase for improvements" OFF)

if(ANALYZE)
  message(STATUS "Analysis on")
  find_program(IWYU_PATH NAMES include-what-you-use iwyu REQUIRED)
  find_program(TIDY_PATH NAMES clang-tidy REQUIRED)
endif(ANALYZE)

find_package(Boost COMPONENTS graph)
find_package(CLI11 REQUIRED)
find_package(fmt REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(RdKafkaCPP REQUIRED)
find_package(spdlog REQUIRED)
find_package(TBB REQUIRED)

pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)

configure_file(
  ${CMAKE_SOURCE_DIR}/include/config.hxx.in
  ${CMAKE_BINARY_DIR}/include/config.hxx @ONLY
)

add_executable(
  ${PROJECT_NAME}
  src/${PROJECT_NAME}.cxx
  src/utils/component.cxx
  src/utils/application.cxx
  src/graph/typedefs.cxx
  src/graph/strategy.cxx
  src/graph/context.cxx
  src/graph/earliest.cxx
)

target_include_directories(
  ${PROJECT_NAME} PUBLIC
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/include
)

if(ANALYZE)
  set_property(
    TARGET ${PROJECT_NAME} PROPERTY CXX_INCLUDE_WHAT_YOU_USE
    ${IWYU_PATH}
  )
  set(CMAKE_CXX_CLANG_TIDY clang-tidy -checks=-*,readability-*)
endif(ANALYZE)

target_link_libraries(
  ${PROJECT_NAME}
  Boost::graph
  CLI11::CLI11
  fmt::fmt
  RdKafkaCPP::RdKafkaCPP
  PkgConfig::GLIB
  PkgConfig::SYSTEMD
  spdlog::spdlog
  tbb
)
