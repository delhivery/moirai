find_package(PkgConfig QUIET)

pkg_search_module(
  PC_rdkafkacpp
  rdkafka++
)

find_path(
  rdkafkacpp_INCLUDE_DIR
  NAMES librdkafka/rdkafkacpp.h
  PATHS ${PC_rdkafkacpp_INCLUDE_DIRS}
)

find_library(
  rdkafkacpp_LIBRARY
  NAMES rdkafka++
  PATHS ${PC_rdkafkacpp_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  RdKafkaCPP
  REQUIRED_VARS rdkafkacpp_INCLUDE_DIR rdkafkacpp_LIBRARY
  VERSION_VAR PC_rdkafkacpp_VERSION
)

if(RdKafkaCPP_FOUND)
  set(RdKafkaCPP_VERSION ${PC_rdkafkacpp_VERSION})
  string(REPLACE "." ";" version_list ${PC_rdkafkacpp_VERSION})
  list(GET version_list 0 RdKafkaCPP_VERSION_MAJOR)
  list(GET version_list 1 RdKafkaCPP_VERSION_MINOR)
  list(GET version_list 2 RdKafkaCPP_VERSION_PATCH)

  if(NOT TARGET RdKafkaCPP::RdKafkaCPP)
    add_library(RdKafkaCPP::RdKafkaCPP UNKNOWN IMPORTED)
    set_target_properties(
      RdKafkaCPP::RdKafkaCPP PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${rdkafkacpp_INCLUDE_DIR}"
      IMPORTED_LINK_INTERFACE_LANGUAGES "CPP"
      IMPORTED_LOCATION "${rdkafkacpp_LIBRARY}"
    )
  endif()
endif()
