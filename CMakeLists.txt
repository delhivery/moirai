cmake_minimum_required(VERSION 3.10)

project(moirai)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_GENERATOR Ninja)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_BUILD_TYPE)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

option(ANALYZE "Analyze codebase for improvements" OFF)
option(LOCAL "Run EP locally with production dataset" OFF)

if(ANALYZE)
  message(STATUS "Analysis on")
  find_program(IWYU_PATH NAMES include-what-you-use iwyu REQUIRED)
  find_program(TIDY_PATH NAMES clang-tidy REQUIRED)
endif(ANALYZE)

find_package(Boost COMPONENTS graph)
find_package(fmt REQUIRED)
find_package(TBB REQUIRED)
find_package(Poco REQUIRED Util Foundation Net NetSSL)
find_package(OpenSSL REQUIRED)
find_package(RdKafkaCPP REQUIRED)

if(NOT LOCAL)
  add_executable(
    ${PROJECT_NAME}
    src/date_utils.cxx
    src/graph_helpers.cxx
    src/transportation.cxx
    src/solver.cxx
    src/utils.cxx
    src/kafka_reader.cxx
    src/search_writer.cxx
    src/processor.cxx
    src/solver_wrapper.cxx
    src/server.cxx
  )
else(NOT LOCAL)
  add_executable(
    ${PROJECT_NAME}
    src/date_utils.cxx
    src/graph_helpers.cxx
    src/transportation.cxx
    src/solver.cxx
    src/utils.cxx
    src/file_reader.cxx
    src/kafka_reader.cxx
    src/search_writer.cxx
    src/solver_wrapper.cxx
    src/server_local.cxx
  )
endif(NOT LOCAL)

target_include_directories(${PROJECT_NAME} PUBLIC include)

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
  fmt::fmt
  tbb
  Poco::Foundation
  Poco::Util
  Poco::Net
  Poco::NetSSL
  RdKafkaCPP::RdKafkaCPP
)
