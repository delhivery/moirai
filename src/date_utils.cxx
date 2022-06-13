#include "date_utils.hxx"
#include <date/date.h>

time_of_day::time_of_day() noexcept
  : mTime(minutes::zero())
{
}

time_of_day::time_of_day(minutes _minutes) noexcept
  : mTime(_minutes % oneDay)
{
}

auto
time_of_day::operator+=(const time_of_day& other) noexcept -> time_of_day&
{
  mTime += other.mTime;
  mTime %= oneDay;
  // mTime = (mTime.to_duration() + other.mTime.to_duration()) % oneDay;
  return *this;
}

auto
time_of_day::to_duration() const noexcept -> minutes
{
  return mTime;
}

auto
time_of_day::operator+(const time_of_day& other) const noexcept -> time_of_day
{
  time_of_day result = *this;
  result += other;
  return result;
}

auto
time_of_day::operator-=(const time_of_day& other) noexcept -> time_of_day&
{
  mTime -= other.mTime;
  mTime %= oneDay;
  return *this;
}

auto
time_of_day::operator-(const time_of_day& other) const noexcept -> time_of_day
{
  time_of_day result = *this;
  result -= other;
  return result;
}

auto
parse_time(std::string_view timeString) -> minutes
{
  minutes result{ 0 };
  std::stringstream timeStream(std::string(timeString), std::ios_base::in);
  timeStream >> date::parse("%R", result);
  return result;
}

auto
parse_date(std::string_view dateString) -> datetime
{
  datetime result;
  std::stringstream dateStream(std::string(dateString), std::ios_base::in);
  dateStream >> date::parse("%F", result);
  return result;
}

auto
parse_datetime(std::string_view dateString) -> datetime
{
  datetime result;
  std::stringstream dateStream(std::string(dateString), std::ios_base::in);
  dateStream >> date::parse("%F %R", result);
  return result;
}
