#include "date_utils.hxx"
#include "transportation.hxx"
#include <cstdint>
#include <cstdlib>
#include <date/date.h>
#include <fmt/format.h>
#include <iostream>
#include <regex>
#include <vector>

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
  if (cost.first == TIME_OF_DAY::max() and cost.second == DURATION::max())
    return start;
  TIME_OF_DAY minutes_start{ start -
                             std::chrono::floor<std::chrono::days>(start) };
  DURATION wait_time{ datemod(cost.first - minutes_start,
                              std::chrono::days{ 1 }) };
  return start + wait_time + cost.second;
}

template<>
CLOCK
CalcualateTraversalCost::operator()<PathTraversalMode::REVERSE>(CLOCK start,
                                                                COST cost) const
{
  if (cost.first == TIME_OF_DAY::max() and cost.second == DURATION::max())
    return start;
  TIME_OF_DAY minutes_start{ start -
                             std::chrono::floor<std::chrono::days>(start) };
  DURATION wait_time{ datemod(minutes_start - cost.first,
                              std::chrono::days{ 1 }) };
  return start - wait_time - cost.second;
}

CLOCK
iso_to_date(const std::string& date_string)
{
  std::stringstream date_stream{ date_string };
  CLOCK clock;
  date_stream >> date::parse("%F %T", clock);
  return clock;
}

CLOCK
iso_to_date(const std::string& date_string, const bool is_offset)
{
  std::string formatted_string{ date_string };

  if (is_offset)
    formatted_string =
      fmt::format("{} {}", date_string.substr(0, 10), "04:00:00");
  std::stringstream date_stream{ formatted_string };
  CLOCK clock;
  date_stream >> date::parse("%F %T", clock);
  return clock;
}

CLOCK
iso_to_date(const std::string& date_string, const TIME_OF_DAY& cutoff)
{
  std::string formatted_string{ date_string };

  formatted_string =
    fmt::format("{} {}", date_string.substr(0, 10), "00:00:00");
  std::stringstream date_stream{ formatted_string };
  CLOCK clock;
  date_stream >> date::parse("%F %T", clock);
  return clock + cutoff - DURATION{ 330 };
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
  std::regex split_day_regex("day");
  const std::vector<std::string> day_parts(
    std::sregex_token_iterator(
      time_string.begin(), time_string.end(), split_day_regex, -1),
    std::sregex_token_iterator());

  uint16_t time_days = 0;

  if (day_parts.size() > 1)
    time_days = std::atoi(day_parts[0].c_str()) * 24 * 60;

  std::regex split_nonday_regex(",");
  std::string nonday_time{ time_string };
  const std::vector<std::string> time_parts(
    std::sregex_token_iterator(
      time_string.begin(), time_string.end(), split_nonday_regex, -1),
    std::sregex_token_iterator());

  if (time_parts.size() > 1)
    nonday_time = time_parts[1];

  std::regex split_time_regex(":");
  const std::vector<std::string> parts(
    std::sregex_token_iterator(
      nonday_time.begin(), nonday_time.end(), split_time_regex, -1),
    std::sregex_token_iterator());
  std::uint16_t time =
    std::atoi(parts[0].c_str()) * 60 + std::atoi(parts[1].c_str());

  return std::chrono::minutes(time + time_days);
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
