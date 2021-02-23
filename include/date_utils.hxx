#ifndef moirai_date_utils
#define moirai_date_utils

#include <chrono>  // for duration, system_clock, time_point
#include <cstdint> // for int16_t, int32_t, uint8_t
#include <ratio>   // for ratio
#include <sstream>
#include <utility> // for pair

typedef std::chrono::duration<std::int16_t, std::ratio<60>> DURATION;
typedef DURATION TIME_OF_DAY;
typedef std::chrono::time_point<
  std::chrono::system_clock,
  std::chrono::duration<std::uint32_t, std::ratio<60>>>
  CLOCK;
typedef std::pair<TIME_OF_DAY, DURATION> COST;

enum PathTraversalMode : std::uint8_t;

struct CalcualateTraversalCost
{
  template<PathTraversalMode>
  CLOCK operator()(CLOCK start, COST cost) const;
};

uint16_t datemod(DURATION, DURATION);

CLOCK
iso_to_date(const std::string&);

CLOCK
iso_to_date(const std::string&, const bool);

int64_t
now_as_int64();

std::chrono::minutes
time_string_to_time(const std::string&);

CLOCK
get_departure(CLOCK, TIME_OF_DAY);
#endif
