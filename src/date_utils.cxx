#include "date_utils.hxx"
#include <date/date.h>

time_of_day::time_of_day() noexcept
  : mTime(minutes::zero())
{
}

time_of_day::time_of_day(minutes _minutes) noexcept
  : mTime(_minutes % ONE_DAY)
{
}

time_of_day&
time_of_day::operator+=(const time_of_day& other) noexcept
{
  mTime += other.mTime;
  mTime %= ONE_DAY;
  // mTime = (mTime.to_duration() + other.mTime.to_duration()) % ONE_DAY;
  return *this;
}

minutes
time_of_day::to_duration() const noexcept
{
  return mTime;
}

const time_of_day
time_of_day::operator+(const time_of_day& other) const noexcept
{
  time_of_day result = *this;
  result += other;
  return result;
}

time_of_day&
time_of_day::operator-=(const time_of_day& other) noexcept
{
  mTime -= other.mTime;
  mTime %= ONE_DAY;
  return *this;
}

const time_of_day
time_of_day::operator-(const time_of_day& other) const noexcept
{
  time_of_day result = *this;
  result -= other;
  return result;
}

datetime
parse_date(std::string_view dateString)
{
  datetime result;
  std::stringstream dateStream(std::string(dateString), std::ios_base::in);
  dateStream >> date::parse("%F", result);
  return result;
}

datetime
parse_datetime(std::string_view dateString)
{
  datetime result;
  std::stringstream dateStream(std::string(dateString), std::ios_base::in);
  dateStream >> date::parse("%F %R", result);
  return result;
}

/*
datetime
get_departure(datetime start, time_of_day departure)
{
  time_of_day minutes_start{ start - floor<days>(start) };
  minutes wait_time{ datemod(departure - minutes_start, days{ 1 }) };
  return start + wait_time;
}
*/
