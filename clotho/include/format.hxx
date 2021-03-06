#ifndef FORMAT_HXX
#define FORMAT_HXX

#if __cpp_lib_format >= 201907L
#include <format>
#else
#include <fmt/core.h>
namespace std {
using fmt::format;
};
#endif

#endif
