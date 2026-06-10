#include "date_utils.hxx"
#include "transportation.hxx"
#include <charconv>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string_view>

namespace {

constexpr auto MINUTES_PER_HOUR = 60;
constexpr auto HOURS_PER_DAY = 24;
constexpr auto MINUTES_PER_DAY = HOURS_PER_DAY * MINUTES_PER_HOUR;
constexpr auto DAYS_PER_WEEK = 7;
constexpr auto MINUTES_PER_WEEK = DAYS_PER_WEEK * MINUTES_PER_DAY;
constexpr auto UNIX_EPOCH_WEEKDAY = 4;
constexpr auto IST_OFFSET_MINUTES = 330;
constexpr auto ISO_DATETIME_LENGTH = 19U;
constexpr auto DATE_DASH_2_OFFSET = 4U;
constexpr auto DATE_DASH_3_OFFSET = 7U;
constexpr auto DATE_TIME_SEPARATOR_OFFSET = 10U;
constexpr auto TIME_HOUR_SEPARATOR_OFFSET = 13U;
constexpr auto TIME_MINUTE_SEPARATOR_OFFSET = 16U;
constexpr auto DATE_ONLY_LENGTH = 10U;

auto parse_int(std::string_view input) -> int {
  int value = 0;
  const auto *begin = input.data();
  const auto *end = begin + input.size();
  const auto [ptr, error] = std::from_chars(begin, end, value);
  if (error != std::errc{} || ptr != end) {
    throw std::runtime_error(
        std::format("Failed to parse integer '{}'", input));
  }

  return value;
}

auto parse_iso_datetime(std::string_view input) -> CLOCK {
  if (input.size() < ISO_DATETIME_LENGTH || input[DATE_DASH_2_OFFSET] != '-' ||
      input[DATE_DASH_3_OFFSET] != '-' ||
      input[DATE_TIME_SEPARATOR_OFFSET] != ' ' ||
      input[TIME_HOUR_SEPARATOR_OFFSET] != ':' ||
      input[TIME_MINUTE_SEPARATOR_OFFSET] != ':') {
    throw std::runtime_error("Failed to parse date string");
  }

  const auto year = std::chrono::year{parse_int(input.substr(0, 4))};
  const auto month =
      std::chrono::month{static_cast<unsigned>(parse_int(input.substr(5, 2)))};
  const auto day =
      std::chrono::day{static_cast<unsigned>(parse_int(input.substr(8, 2)))};
  const auto hour = parse_int(input.substr(11, 2));
  const auto minute = parse_int(input.substr(14, 2));
  const auto second = parse_int(input.substr(17, 2));

  const std::chrono::year_month_day ymd{year, month, day};
  if (!ymd.ok()) {
    throw std::runtime_error("Failed to parse date string");
  }

  const auto day_point = std::chrono::sys_days{ymd};
  const auto time_point = day_point + std::chrono::hours{hour} +
                          std::chrono::minutes{minute} +
                          std::chrono::seconds{second};
  return std::chrono::time_point_cast<CLOCK::duration>(time_point);
}

auto minute_of_week(CLOCK timestamp) -> DURATION {
  const auto day = std::chrono::floor<std::chrono::days>(timestamp);
  const auto days_since_epoch = day.time_since_epoch().count();
  const auto weekday = (days_since_epoch + UNIX_EPOCH_WEEKDAY) % DAYS_PER_WEEK;
  const TIME_OF_DAY minute_of_day{timestamp - day};
  return DURATION{static_cast<std::int16_t>((weekday * MINUTES_PER_DAY) +
                                            minute_of_day.count())};
}

template <PathTraversalMode M>
auto scheduled_wait_time(DURATION start_minute_of_week, const COST &cost)
    -> DURATION {
  DURATION best_wait{MINUTES_PER_WEEK};
  for (std::uint8_t day = 0; day < DAYS_PER_WEEK; ++day) {
    if ((cost.days_of_week & (1U << day)) == 0) {
      continue;
    }

    const DURATION scheduled_minute{
        datemod(DURATION{static_cast<std::int16_t>(day * MINUTES_PER_DAY)} +
                    cost.schedule_offset,
                std::chrono::days{DAYS_PER_WEEK})};
    const DURATION wait{
        datemod(M == PathTraversalMode::FORWARD
                    ? scheduled_minute - start_minute_of_week
                    : start_minute_of_week - scheduled_minute,
                std::chrono::days{DAYS_PER_WEEK})};
    if (wait < best_wait) {
      best_wait = wait;
    }
  }

  return best_wait;
}

} // namespace

auto datemod(DURATION lhs, DURATION rhs) -> std::uint16_t {
  std::int16_t count_lhs = lhs.count();
  std::int16_t count_rhs = rhs.count();

  return ((count_lhs % count_rhs) + count_rhs) % count_rhs;
}

template <>
auto CalcualateTraversalCost::operator()<PathTraversalMode::FORWARD>(
    CLOCK start, COST cost) const -> CLOCK {
  if (cost.unreachable || cost.days_of_week == 0) {
    return start;
  }
  const DURATION wait_time = scheduled_wait_time<PathTraversalMode::FORWARD>(
    minute_of_week(start), cost);
  return start + wait_time + cost.duration;
}

template <>
auto CalcualateTraversalCost::operator()<PathTraversalMode::REVERSE>(
    CLOCK start, COST cost) const -> CLOCK {
  if (cost.unreachable || cost.days_of_week == 0) {
    return start;
  }
  const DURATION wait_time = scheduled_wait_time<PathTraversalMode::REVERSE>(
    minute_of_week(start), cost);
  return start - wait_time - cost.duration;
}

auto iso_to_date(const std::string &date_string) -> CLOCK {
  return parse_iso_datetime(date_string);
}

auto iso_to_date(const std::string &date_string, const bool is_offset)
    -> CLOCK {
  std::string formatted_string{date_string};

  if (is_offset) {
    formatted_string = std::format(
        "{} {}", date_string.substr(0, DATE_ONLY_LENGTH), "04:00:00");
  }

  return parse_iso_datetime(formatted_string);
}

auto iso_to_date(const std::string &date_string, const TIME_OF_DAY &cutoff)
    -> CLOCK {
  const std::string formatted_string =
      std::format("{} {}", date_string.substr(0, DATE_ONLY_LENGTH), "00:00:00");
  return parse_iso_datetime(formatted_string) + cutoff -
         DURATION{IST_OFFSET_MINUTES};
}

auto now_as_int64() -> int64_t {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

auto format_clock(const CLOCK &timestamp) -> std::string {
  return std::format("{:%m/%d/%y %H:%M:%S}", timestamp);
}

auto time_string_to_time(std::string_view input) -> std::chrono::minutes {
  int day_minutes = 0;

  if (const auto comma_position = input.find(',');
      comma_position != std::string_view::npos) {
    const auto day_part = input.substr(0, comma_position);
    const auto day_end = day_part.find(' ');
    if (day_end != std::string_view::npos) {
      day_minutes = parse_int(day_part.substr(0, day_end)) * MINUTES_PER_DAY;
    }
    input = input.substr(comma_position + 1);
  }

  while (!input.empty() && input.front() == ' ') {
    input.remove_prefix(1);
  }

  const auto separator = input.find(':');
  if (separator == std::string_view::npos) {
    throw std::runtime_error("Failed to parse time string");
  }

  const auto hours = parse_int(input.substr(0, separator));
  const auto minutes = parse_int(input.substr(separator + 1, 2));
  return std::chrono::minutes(day_minutes + (hours * MINUTES_PER_HOUR) +
                              minutes);
}

auto get_departure(CLOCK start, TIME_OF_DAY departure) -> CLOCK {
  TIME_OF_DAY minutes_start{start -
                            std::chrono::floor<std::chrono::days>(start)};
  DURATION wait_time{datemod(departure - minutes_start, std::chrono::days{1})};
  return start + wait_time;
}
