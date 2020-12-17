#ifndef moirai_date_utils
#define moirai_date_utils

#include <chrono>
#include <cstdint>
#include <ratio>

#include <date/date.h>

typedef std::chrono::duration<std::int16_t, std::ratio<60>> DURATION;
typedef DURATION TIME_OF_DAY;
typedef std::chrono::time_point<
  std::chrono::system_clock,
  std::chrono::duration<std::int32_t, std::ratio<60>>>
  CLOCK;
typedef std::pair<TIME_OF_DAY, DURATION> COST;

enum PathTraversalMode : std::uint8_t;

struct CalcualateTraversalCost
{
  template<PathTraversalMode>
  CLOCK operator()(CLOCK start, COST cost) const;
};

#endif
