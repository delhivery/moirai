#pragma once

#include <simdjson.h>

#if !defined(MOIRAI_SIMDJSON_VERSION_MAJOR) ||                              \
    !defined(MOIRAI_SIMDJSON_VERSION_MINOR) ||                              \
    !defined(MOIRAI_SIMDJSON_VERSION_REVISION)
#error "Moirai must be built through CMake so the selected simdjson version is defined"
#endif

// Ensure the module-facing header is the one selected by CMake. A mismatched
// simdjson header/library pair can silently corrupt DOM string views.
static_assert(simdjson::SIMDJSON_VERSION_MAJOR == MOIRAI_SIMDJSON_VERSION_MAJOR &&
                  simdjson::SIMDJSON_VERSION_MINOR ==
                      MOIRAI_SIMDJSON_VERSION_MINOR &&
                  simdjson::SIMDJSON_VERSION_REVISION ==
                      MOIRAI_SIMDJSON_VERSION_REVISION,
              "simdjson header version does not match the CMake-selected "
              "simdjson dependency");
