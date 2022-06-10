#include "date_utils.hxx"
#include "transportation.hxx"

std::uint16_t
datemod(DURATION_MINUTES lhs, DURATION_MINUTES rhs)
{
  std::int16_t count_lhs = lhs.count();
  std::int16_t count_rhs = rhs.count();

  return ((count_lhs % count_rhs) + count_rhs) % count_rhs;
}

TemporalEdgeCostAttributes::TemporalEdgeCostAttributes(
  const DURATION_MINUTES& loading,
  const TIME_OF_DAY_MINUTES& departure,
  const DURATION_MINUTES& duration,
  const DURATION_MINUTES& unloading,
  const std::vector<uint8_t>& departureDays)
  : mDuration(duration)
  , mTransient(false)
{
  mDeparture = hh_mm_ss<minutes>(departure - loading);
  mArrival = datemod(departure + duration + unloading, days(1));

  for (const auto& departureDay : departureDays)
    mDepartureDays |= 1 << sizeof(mDepartureDays) * CHAR_BIT - departureDay - 2;
  mArrivalDays = mDepartureDays;

  if (mDeparture + mDuration + mUnloading > days(1)) {
    mArrivalDays = mDepartureDays >> 1;
    uint8_t lwd = mDepartureDays & 1;
    lwd = lwd << 6;
    mArrivalDays |= lwd;
  }
}

auto
TemporalEdgeCostAttributes::transient() const -> bool
{
  return mTransient;
}

template<>
int8_t
TemporalEdgeCostAttributes::next_working_day<PathTraversalMode::FORWARD>(
  const weekday& startDay) const
{
  uint8_t toShift = startDay.c_encoding();
  uint8_t lShift = mDepartureDays << toShift + 1;
  lShift >>= 1;
  uint8_t rShift = mDepartureDays >> 7 - toShift;
  uint8_t result = lShift | rShift;
  return std::countl_zero(result) - 1;
}

template<>
int8_t
TemporalEdgeCostAttributes::next_working_day<PathTraversalMode::REVERSE>(
  const weekday& startDay) const
{
  uint8_t toShift = startDay.c_encoding();
  uint8_t rShift = mDepartureDays >> CHAR_BIT - toShift - 2;
  uint8_t lShift = mDepartureDays << toShift + 2;
  lShift >>= 1;
  uint8_t result = lShift | rShift;
  return std::countr_zero(result);
};

template<>
CLOCK_MINUTES
EdgeTraversalCost::operator()<PathTraversalMode::FORWARD>(
  const CLOCK_MINUTES& arrival,
  const TemporalEdgeCostAttributes& costAttrs) const
{
  if (costAttrs.transient())
    return arrival;
  auto arrivalDay = floor<days>(arrival);
  auto arrivalWeekday = weekday(sys_days(arrivalDay));
  auto arrivalTime = arrival - arrivalDay;

  DURATION_MINUTES timeIdle(0);
  days baseOffset(0), daysIdle(0);

  // If departure earlier than start time, then we need to at least wait until
  // next day to depart. So we need to find the next available working day
  // starting stary_day +1
  if (arrivalTime > costAttrs.mDeparture)
    baseOffset = days(1);
  daysIdle = days(costAttrs.next_working_day<PathTraversalMode::FORWARD>(
    arrivalWeekday + baseOffset));

  timeIdle = baseOffset + daysIdle + costAttrs.mDeparture - arrivalTime;
  return arrival + timeIdle + costAttrs.mDuration;
}

template<>
CLOCK_MINUTES
EdgeTraversalCost::operator()<PathTraversalMode::REVERSE>(
  const CLOCK_MINUTES& arrival,
  const TemporalEdgeCostAttributes& costAttrs) const
{
  if (costAttrs.transient())
    return arrival;
  auto arrivalDay = floor<days>(arrival);
  auto arrivalWeekday = weekday(sys_days(arrivalDay));
  auto arrivalTime = arrival - arrivalDay;

  DURATION_MINUTES timeIdle(0);
  days baseOffset(0), daysIdle(0);

  if (arrivalTime < costAttrs.arrival())
    baseOffset = days(1);
  daysIdle = days(costAttrs.next_working_day<PathTraversalMode::REVERSE>(
    arrivalWeekday - baseOffset));
  timeIdle = baseOffset + daysIdle + arrivalTime - costAttrs.mArrival;
  return arrival - timeIdle - costAttrs.mDuration - costAttrs.mUnloading;
}

template<>
CLOCK_MINUTES
EdgeTraversalCost::operator()<PathTraversalMode::REVERSE>(CLOCK_MINUTES start,
                                                          COST cost) const
{
  if (cost.first == TIME_OF_DAY::max() and cost.second == DURATION::max())
    return start;
  DURATION_MINUTES wait_time{ datemod(minutes_start - cost.first, days{ 1 }) };
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
  return system_clock::now().time_since_epoch().count() / 1000 / 1000;
}

minutes
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

  return minutes(time + time_days);
}

CLOCK_MINUTES
get_departure(CLOCK_MINUTES start, TIME_OF_DAY_IN_MINUTES departure)
{
  TIME_OF_DAY_IN_MINUTES minutes_start{ start - floor<days>(start) };
  DURATION_MINUTES wait_time{ datemod(departure - minutes_start, days{ 1 }) };
  return start + wait_time;
}
