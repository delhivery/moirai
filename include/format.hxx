#ifndef MOIRAI_FORMAT
#define MOIRAI_FORMAT

#ifdef __cpp_lib_format
#include <format>
#else
#include <fmt/core.h>
namespace std {
using fmt::format;
};
#endif

#endif
