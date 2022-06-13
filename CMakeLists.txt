cmake_minimum_required(VERSION 3.10)

project(moirai)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_GENERATOR Ninja)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -fuse-ld=mold") Treat warnings
# as errors: Werror Stop compiling on first error: Wfatal-errors
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wfatal-errors")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
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

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libstdc++")
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto=auto")
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -fwhole-program -flto=auto -fgraphite-identity -floop-nest-optimize -fno-semantic-interposition -fno-plt"
    )
  else()
    message(FATAL_ERROR "Unsupported compiler")
  endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

option(NODE_FILE "Load `nodes` from file" OFF)
option(EDGE_FILE "Load `edges` from file" OFF)
option(LOAD_FILE "Load `loads` from file" OFF)
option(SYNC_OFF "Disable sync to elasticsearch" OFF)
option(LINT "Verify codebase for styling errors" OFF)

find_package(Boost COMPONENTS graph)
find_package(TBB REQUIRED)
find_package(fmt REQUIRED)
find_package(TBB REQUIRED)
find_package(Poco REQUIRED Util Foundation Net NetSSL)
find_package(OpenSSL REQUIRED)
find_package(RdKafkaCPP REQUIRED)

add_executable(
  ${PROJECT_NAME}
  src/date_utils.cxx
  src/edge_cost_attributes.cxx
  src/graph_helpers.cxx
  src/transportation.cxx
  src/solver.cxx
  src/utils.cxx
  src/scan_reader.cxx
  src/kafka_reader.cxx
  src/search_writer.cxx
  src/file_reader.cxx
  src/processor.cxx
  src/solver_wrapper.cxx
  src/server.cxx)

target_include_directories(${PROJECT_NAME} PUBLIC include)

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

target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE Boost::graph
          Poco::Foundation
          Poco::Util
          Poco::Net
          Poco::NetSSL
          RdKafkaCPP::RdKafkaCPP
          TBB::tbb
          fmt::fmt-header-only)
