#pragma once

#include <chrono>  // for duration, system_clock, time_point
#include <cstdint> // for int16_t, int32_t, uint8_t
#include <ratio>   // for ratio
#include <string>
#include <string_view>
#include <utility> // for pair

inline constexpr std::intmax_t SECONDS_PER_MINUTE = 60;
using minute_ratio = std::ratio<SECONDS_PER_MINUTE>;
using DURATION = std::chrono::duration<std::int16_t, minute_ratio>;
using TIME_OF_DAY = DURATION;
using CLOCK =
    std::chrono::time_point<std::chrono::system_clock,
                            std::chrono::duration<std::uint32_t, minute_ratio>>;
using COST = std::pair<TIME_OF_DAY, DURATION>;

enum PathTraversalMode : std::uint8_t;

struct CalcualateTraversalCost {
  template <PathTraversalMode>
  auto operator()(CLOCK start, COST cost) const -> CLOCK;
};

auto datemod(DURATION lhs, DURATION rhs) -> uint16_t;

auto iso_to_date(const std::string &date_string) -> CLOCK;

auto iso_to_date(const std::string &date_string, bool is_offset) -> CLOCK;

auto iso_to_date(const std::string &date_string, const TIME_OF_DAY &cutoff)
    -> CLOCK;

auto now_as_int64() -> int64_t;

auto time_string_to_time(std::string_view time_string) -> std::chrono::minutes;

auto get_departure(CLOCK start, TIME_OF_DAY departure) -> CLOCK;

auto format_clock(const CLOCK &timestamp) -> std::string;
