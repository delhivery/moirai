export module moirai.date_utils;

export import std;

export inline constexpr std::intmax_t SECONDS_PER_MINUTE = 60;
export using minute_ratio = std::ratio<SECONDS_PER_MINUTE>;
export using DURATION = std::chrono::duration<std::int16_t, minute_ratio>;
export using TIME_OF_DAY = DURATION;
export using CLOCK =
    std::chrono::time_point<std::chrono::system_clock,
                            std::chrono::duration<std::uint32_t, minute_ratio>>;

export struct COST {
  DURATION schedule_offset{};
  DURATION duration{};
  std::uint8_t days_of_week{};
  bool unreachable = false;
};

export enum PathTraversalMode : std::uint8_t {
  FORWARD = 0,
  REVERSE = 1,
};

export struct CalcualateTraversalCost {
  template <PathTraversalMode>
  auto operator()(CLOCK start, COST cost) const -> CLOCK;
};

export auto datemod(DURATION lhs, DURATION rhs) -> std::uint16_t;

export auto iso_to_date(const std::string& date_string) -> CLOCK;

export auto iso_to_date(const std::string& date_string, bool is_offset)
    -> CLOCK;

export auto iso_to_date(const std::string& date_string,
                        const TIME_OF_DAY& cutoff) -> CLOCK;

export auto iso_to_date_utc_cutoff(const std::string& date_string,
                                   const TIME_OF_DAY& cutoff) -> CLOCK;

export auto now_as_int64() -> std::int64_t;

export auto time_string_to_time(std::string_view time_string)
    -> std::chrono::minutes;

export auto get_departure(CLOCK start, TIME_OF_DAY departure) -> CLOCK;

export auto format_clock(const CLOCK& timestamp) -> std::string;
