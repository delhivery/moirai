#ifndef moirai_date_utils
#define moirai_date_utils

#include <chrono>
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
  static constexpr std::chrono::days oneDay{ 1 };

public:
  time_of_day() noexcept;

  time_of_day(minutes _minutes) noexcept;

  [[nodiscard]] auto to_duration() const noexcept -> minutes;

  auto operator+=(const time_of_day& other) noexcept -> time_of_day&;

  auto operator+(const time_of_day& other) const noexcept -> time_of_day;

  auto operator-=(const time_of_day& other) noexcept -> time_of_day&;

  auto operator-(const time_of_day& other) const noexcept -> time_of_day;

  auto operator<=>(const time_of_day& other) const
    -> std::strong_ordering = default;
};

auto parse_time(std::string_view) -> minutes;

auto parse_date(std::string_view) -> datetime;

auto parse_datetime(std::string_view) -> datetime;

#endif
