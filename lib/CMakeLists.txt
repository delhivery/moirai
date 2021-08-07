add_library(lib${PROJECT_NAME} ${PROJECT_NAME}.cxx)

target_link_libraries(
  lib${PROJECT_NAME}
  ${PROJECT_NAME}_HEADERS
  fmt::fmt
)
