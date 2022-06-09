#include "date_utils.hxx"
#include "transportation.hxx"

std::uint16_t
datemod(DURATION_MINUTES lhs, DURATION_MINUTES rhs)
{
  std::int16_t count_lhs = lhs.count();
  std::int16_t count_rhs = rhs.count();

  return ((count_lhs % count_rhs) + count_rhs) % count_rhs;
}

TemporalEdgeCostAttributes
  : TemporalEdgeCostAttributes(const TIME_OF_DAY_MINUTES& departure,
                               const DURATION_MINUTES& duration,
                               const std::vector<int>& working_days)
  : m_departure(departure)
  , m_duration(duration)
  , m_transient(false)
{
  for (const auto working_day : working_days)
    m_working_days |=
      1 << (sizeof(m_working_days) * CHAR_BIT - working_day - 2);
}

template<>
uint8_t
TemporalEdgeCostAttributes::next_working_day<PathTraversalMode::FORWARD>(
  const weekday& start_day) const
{
  uint8_t to_shift = start_day.c_encoding();
  uint8_t rshift = m_working_days << to_shift + 1;
  uint8_t lshift = m_working_days >> 7 - to_shift;
  uint8_t result = rshift | lshift;
  return std::countl_zero(result) - 1;
}

template<>
uint8_t TemporalEdgeCostAttributes::next_working_day<PathTraversalMode::REVERSE>(const weekday& start_day) const {
  
};

template<>
CLOCK_MINUTES
EdgeTraversalCost::operator()<PathTraversalMode::FORWARD>(
  const CLOCK_MINUTES& start,
  const TemporalEdgeCostAttributes& cost_attrs) const
{
  if (cost_attrs.m_transient)
    return start;

  auto start_day = floor<days>(start);
  auto start_day_of_week = weekday(sys_days(start_day));

  auto start_minutes = start - start_day;

  DURATION_MINUTES time_idle = 0;
  std::chrono::days base_offset = 0, days_idle = 0;

  // If departure earlier than start time, then we need to at least wait until
  // next day to depart. So we need to find the next available working day
  // starting stary_day +1
  if (start_minutes > cost_attrs.m_departure)
    base_offset = 1;
  days_idle = cost_attrs.next_working_day(start_day_of_week + base_offset);

  time_idle = base_offset + days_idle + cost_attrs.m_departure - start_minutes;
  return start + time_idle + cost_attrs.m_duration;
}

template<>
CLOCK_MINUTES
EdgeTraversalCost::operator()<PathTraversalMode::REVERSE>(
  const CLOCK_MINUTES& start,
  const TemporalEdgeCostAttributes& cost_attrs) const
{
  if (cost_attrs.m_transient)
    return start;

  auto start_day = floor<days>(start);
  auto start_day_of_week = weekday(sys_days(start_day));
  auto start_minutes = start - start_day;
}

template<>
CLOCK_MINUTES
EdgeTraversalCost::operator()<PathTraversalMode::REVERSE>(CLOCK_MINUTES start,
                                                          COST cost) const
{
  if (cost.first == TIME_OF_DAY::max() and cost.second == DURATION::max())
    return start;
  DURATION_MINUTES wait_time{ datemod(minutes_start - cost.first,
                                      std::chrono::days{ 1 }) };
  return start - wait_time - cost.second;
}

CLOCK_MINUTES
iso_to_date(const std::string& date_string)
{
  std::stringstream date_stream{ date_string };
  CLOCK_MINUTES clock;
  date_stream >> date::parse("%F %T", clock);
  return clock;
}

CLOCK_MINUTES
iso_to_date(const std::string& date_string, const bool is_offset)
{
  std::string formatted_string{ date_string };

  if (is_offset)
    formatted_string =
      fmt::format("{} {}", date_string.substr(0, 10), "04:00:00");
  std::stringstream date_stream{ formatted_string };
  CLOCK_MINUTES clock;
  date_stream >> date::parse("%F %T", clock);
  return clock;
}

CLOCK_MINUTES
iso_to_date(const std::string& date_string,
            const TIME_OF_DAY_IN_MINUTES& cutoff)
{
  std::string formatted_string{ date_string };

  formatted_string =
    fmt::format("{} {}", date_string.substr(0, 10), "00:00:00");
  std::stringstream date_stream{ formatted_string };
  CLOCK_MINUTES clock;
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

CLOCK_MINUTES
get_departure(CLOCK_MINUTES start, TIME_OF_DAY_IN_MINUTES departure)
{
  TIME_OF_DAY_IN_MINUTES minutes_start{
    start - std::chrono::floor<std::chrono::days>(start)
  };
  DURATION_MINUTES wait_time{ datemod(departure - minutes_start,
                                      std::chrono::days{ 1 }) };
  return start + wait_time;
}
