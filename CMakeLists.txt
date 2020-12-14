cmake_minimum_required(VERSION 3.19)

project(moirai)

set(CMAKE_CXX_STANDARD 20)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

option(ANALYZE "Analyze codebase for improvements" OFF)

if(ANALYZE)
  message(STATUS "Analysis on")
  find_program(IWYU_PATH NAMES include-what-you-use iwyu REQUIRED)
  find_program(TIDY_PATH NAMES clang-tidy REQUIRED)
endif(ANALYZE)

find_package(Boost COMPONENTS graph)
find_package(nlohmann_json REQUIRED)
find_package(fmt REQUIRED)
find_package(TBB REQUIRED)

# add_library(${PROJECT_NAME}Headers INTERFACE)
# target_include_directories(${PROJECT_NAME}Headers INTERFACE include/)
# 
# add_executable(
#   ${PROJECT_NAME}
#   src/moirai.cxx
#   src/graph.cxx
#   src/optimal.cxx
# )
# 
add_library(${PROJECT_NAME} src/moiraiv2.cxx)

if(ANALYZE)
  set_property(
    TARGET ${PROJECT_NAME} PROPERTY CXX_INCLUDE_WHAT_YOU_USE
    ${IWYU_PATH}
  )
  set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=*")
endif(ANALYZE)


target_link_libraries(
  ${PROJECT_NAME}
  ${PROJECT_NAME}Headers
  Boost::graph
  nlohmann_json::nlohmann_json
  fmt::fmt
  tbb
)
