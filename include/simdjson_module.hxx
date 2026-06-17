#pragma once

#include <simdjson.h>

// Ensure the header version matches the linked library. A mismatch
// (e.g., 4.6.2 header with 3.12.3 .so) causes silent ABI corruption.
static_assert(simdjson::SIMDJSON_VERSION_MAJOR == 4 &&
              simdjson::SIMDJSON_VERSION_MINOR >= 6,
              "simdjson >= 4.6 required. Ensure the header and shared library "
              "versions match (check: simdjson --version vs /usr/include/simdjson.h).");
