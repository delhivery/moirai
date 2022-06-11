#ifndef moirai_date_utils
#define moirai_date_utils

#include <chrono>  // for duration, system_clock, time_point
#include <cstdint> // for int16_t, int32_t, uint8_t
#include <string>
#include <string_view>

using datetime =
  std::chrono::time_point<std::chrono::system_clock, std::chrono::minutes>;
using days = std::chrono::days;
using minutes = std::chrono::minutes;
using weekday = std::chrono::weekday;

class time_of_day
{
private:
  minutes mTime;
  static constexpr std::chrono::days ONE_DAY{ 1 };

public:
  time_of_day() noexcept;

  time_of_day(minutes _minutes) noexcept;

  minutes to_duration() const noexcept;

  time_of_day& operator+=(const time_of_day& other) noexcept;

  const time_of_day operator+(const time_of_day& other) const noexcept;

  time_of_day& operator-=(const time_of_day& other) noexcept;

  const time_of_day operator-(const time_of_day& other) const noexcept;

  auto operator<=>(const time_of_day& other) const = default;
};

datetime parse_date(std::string_view);

datetime parse_datetime(std::string_view);

/*
datetime
iso_to_date(std::string_view, const bool);

datetime
iso_to_date(std::string_view, const time_of_day&);

int64_t
now_as_int64();
*/

#endif
