#include "date_utils.hxx"
#include "transportation.hxx"
#include <cstdint>
#include <cstdlib>
#include <date/date.h>
#include <iostream>
#include <regex>
#include <vector>

#ifdef __cpp_lib_format
#include <format>
#else
#include <fmt/core.h>
namespace std {
using fmt::format;
}
#endif

std::uint16_t
datemod(DURATION lhs, DURATION rhs)
{
  std::int16_t count_lhs = lhs.count();
  std::int16_t count_rhs = rhs.count();

  return ((count_lhs % count_rhs) + count_rhs) % count_rhs;
}

template<>
CLOCK
CalcualateTraversalCost::operator()<PathTraversalMode::FORWARD>(CLOCK start,
                                                                COST cost) const
{
  TIME_OF_DAY minutes_start{ start -
                             std::chrono::floor<std::chrono::days>(start) };
  DURATION wait_time{ datemod(cost.first - minutes_start,
                              std::chrono::days{ 1 }) };
  /*
  std::cout << std::format(
                 "Departing using edge starting {} arrives at target on {}. "
                 "Cost: {}, {}",
                 start.time_since_epoch().count() * 60,
                 (start + wait_time + cost.second).time_since_epoch().count() *
                   60,
                 cost.first.count(),
                 cost.second.count())
            << std::endl;
  */
  return start + wait_time + cost.second;
}

template<>
CLOCK
CalcualateTraversalCost::operator()<PathTraversalMode::REVERSE>(CLOCK start,
                                                                COST cost) const
{

  TIME_OF_DAY minutes_start{ start -
                             std::chrono::floor<std::chrono::days>(start) };
  DURATION wait_time{ datemod(minutes_start - cost.first,
                              std::chrono::days{ 1 }) };
  return start - wait_time - cost.second;
}

CLOCK
iso_to_date(std::string date_string)
{
  std::stringstream date_stream{ date_string };
  CLOCK clock;
  date_stream >> date::parse("%F %T", clock);
  return clock;
}

int64_t
now_as_int64()
{
  return std::chrono::system_clock::now().time_since_epoch().count() / 1000 /
         1000;
}

std::chrono::minutes
time_string_to_time(const std::string& time_string)
{
  std::regex split_time_regex(":");
  const std::vector<std::string> parts(
    std::sregex_token_iterator(
      time_string.begin(), time_string.end(), split_time_regex, -1),
    std::sregex_token_iterator());
  std::uint16_t time =
    std::atoi(parts[0].c_str()) * 60 + std::atoi(parts[1].c_str());

  return std::chrono::minutes(time);
}

CLOCK
get_departure(CLOCK start, TIME_OF_DAY departure)
{
  TIME_OF_DAY minutes_start{ start -
                             std::chrono::floor<std::chrono::days>(start) };
  DURATION wait_time{ datemod(departure - minutes_start,
                              std::chrono::days{ 1 }) };
  return start + wait_time;
}
