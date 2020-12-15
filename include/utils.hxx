#ifndef moirai_utils
#define moirai_utils

#include <chrono>
#include <cstdint>
#include <ratio>

using DURATION = std::chrono::duration<int16_t, std::ratio<60>>;
using CLOCK =
  std::chrono::time_point<std::chrono::system_clock,
                          std::chrono::duration<int32_t, std::ratio<60>>>;

#endif
