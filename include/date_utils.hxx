#ifndef moirai_date_utils
#define moirai_date_utils

#include <chrono> // for duration, system_clock, time_point
#include <concepts>
#include <cstdint> // for int16_t, int32_t, uint8_t
#include <ratio>   // for ratio

using namespace std::chrono;

using DURATION_MINUTES = duration<std::int32_t, std::ratio<60>>;

using TIME_OF_DAY_MINUTES = DURATION_MINUTES;

using CLOCK_MINUTES = time_point<system_clock, DURATION_MINUTES>;

enum PathTraversalMode : std::uint8_t;

enum DAY_OF_WEEK : std::uint8_t
{
  SUN = 0x00000001,
  MON = 0x00000010,
  TUE = 0x00000100,
  WED = 0x00001000,
  THU = 0x00010000,
  FRI = 0x00100000,
  SAT = 0x01000000,
};

struct TemporalEdgeCost
{
  TIME_OF_DAY_MINUTES m_departure;
  DURATION_MINUTES m_duration;
  uint8_t m_working_days = 0x01111111;
  bool m_transient;
}

struct EdgeTraversalCost
{
  template<PathTraversalMode>
  CLOCK_MINUTES operator()(const CLOCK_MINUTES&, const TemporalEdgeCost&) const;
};

uint16_t datemod(DURATION_MINUTES, DURATION_MINUTES);

CLOCK_MINUTES
iso_to_date(const std::string&);

CLOCK_MINUTES
iso_to_date(const std::string&, const bool);

CLOCK_MINUTES
iso_to_date(const std::string&, const TIME_OF_DAY_MINUTES&);

int64_t
now_as_int64();

DURATION_MINUTES
time_string_to_time(const std::string&);

CLOCK_MINUTES
get_departure(CLOCK_MINUTES, TIME_OF_DAY_MINUTES);
#endif
