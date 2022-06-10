#ifndef moirai_date_utils
#define moirai_date_utils

#include <chrono> // for duration, system_clock, time_point
#include <climits>
#include <concepts>
#include <cstdint> // for int16_t, int32_t, uint8_t
#include <ratio>   // for ratio
#include <vector>

using namespace std::chrono;

using DURATION_MINUTES = duration<std::int32_t, std::ratio<60>>;

using TIME_OF_DAY_MINUTES = DURATION_MINUTES;

using CLOCK_MINUTES = time_point<system_clock, DURATION_MINUTES>;

constexpr DURATION_MINUTES HOUR = DURATION_MINUTES(60);

constexpr DURATION_MINUTES DAY = DURATION_MINUTES(24 * 60);

constexpr DURATION_MINUTES IST = DURATION_MINUTES(330);

enum PathTraversalMode : std::uint8_t;

class TemporalEdgeCostAttributes
{
private:
  // mDeparture = departure - loading
  hh_mm_ss<minutes> mDeparture;
  // mArrival = arrival + unloading = loading + departure + duration + unloading
  hh_mm_ss<minutes> mArrival;
  minutes mDuration;
  uint8_t mDepartureDays = 0;
  uint8_t mArrivalDays = 0;
  bool mTransient = false;

public:
  TemporalEdgeCostAttributes() = default;

  TemporalEdgeCostAttributes(const DURATION_MINUTES&,
                             const TIME_OF_DAY_MINUTES&,
                             const DURATION_MINUTES&,
                             const DURATION_MINUTES&,
                             const std::vector<uint8_t>&);

  template<PathTraversalMode>
  int8_t next_working_day(const weekday&) const;

  auto transient() const -> bool;
};

struct EdgeTraversalCost
{
  template<PathTraversalMode>
  CLOCK_MINUTES operator()(const CLOCK_MINUTES&,
                           const TemporalEdgeCostAttributes&) const;
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
