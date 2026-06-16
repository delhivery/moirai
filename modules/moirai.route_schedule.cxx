export module moirai.route_schedule;

export import std;
import moirai.date_utils;
import moirai.json_utils;
import moirai.transportation;

export struct RouteEdgeSpec {
  std::string source_center_code;
  std::string target_center_code;
  TransportEdge edge;
};

export inline auto parse_route_days_of_week(const moirai::Json& route)
    -> std::uint8_t {
  const auto* days = moirai::find_array_member(route, "days_of_week");
  if (days == nullptr || moirai::json_size(*days) == 0) {
    return ALL_DAYS_OF_WEEK;
  }

  std::uint8_t mask = 0;
  for (const auto& day : *days) {
    const auto value = moirai::get_integer<std::int16_t>(day);
    if (!value.has_value()) {
      continue;
    }

    const auto normalized_day = ((*value % 7) + 7) % 7;
    mask |= static_cast<std::uint8_t>(1U << normalized_day);
  }

  return mask == 0 ? ALL_DAYS_OF_WEEK : mask;
}

export inline auto lower_copy(std::string_view value) -> std::string {
  std::string lowered{value};
  std::ranges::transform(lowered, lowered.begin(), [](unsigned char input) {
    return static_cast<char>(std::tolower(input));
  });
  return lowered;
}

export struct RouteStopSpec {
  std::optional<std::string_view> center_code;
  std::optional<TIME_OF_DAY> relative_arrival;
  std::optional<TIME_OF_DAY> relative_departure;
};

export inline auto build_route_edge_specs(const moirai::Json& route,
                                          DURATION ist_offset)
    -> std::vector<RouteEdgeSpec> {
  const auto uuid = moirai::find_string_member(route, "route_schedule_uuid");
  const auto name = moirai::find_string_member(route, "name");
  const auto route_type_value = moirai::find_string_member(route, "route_type");
  const auto reporting_time =
      moirai::find_string_member(route, "reporting_time");
  const auto* stops = moirai::find_array_member(route, "halt_centers");
  if (!uuid.has_value() || !name.has_value() ||
      !route_type_value.has_value() || !reporting_time.has_value() ||
      stops == nullptr) {
    return {};
  }

  const auto route_type = lower_copy(*route_type_value);
  const DURATION reporting_offset =
      std::chrono::duration_cast<TIME_OF_DAY>(
          time_string_to_time(*reporting_time)) -
      ist_offset;
  const auto scheduled_days = parse_route_days_of_week(route);

  std::vector<moirai::Json> loading_stop_values;
  loading_stop_values.reserve(moirai::json_size(*stops));
  for (const auto& stop : *stops) {
    const auto* loading_allowed = moirai::find_member(stop, "loading_allowed");
    if (loading_allowed != nullptr) {
      const auto bool_value = moirai::get_bool(*loading_allowed);
      if (bool_value.has_value() && !*bool_value) {
        continue;
      }
    }

    loading_stop_values.push_back(stop);
  }

  std::vector<RouteStopSpec> loading_stops;
  loading_stops.reserve(loading_stop_values.size());
  for (std::size_t index = 0; index < loading_stop_values.size(); ++index) {
    const auto& stop = loading_stop_values[index];
    RouteStopSpec spec{
        .center_code = moirai::find_string_member(stop, "center_code"),
        .relative_arrival = std::nullopt,
        .relative_departure = std::nullopt};
    if (spec.center_code.has_value()) {
      if (index > 0) {
        if (const auto arrival = moirai::find_string_member(stop, "rel_eta");
            arrival.has_value()) {
          spec.relative_arrival =
              std::chrono::duration_cast<TIME_OF_DAY>(
                  time_string_to_time(*arrival));
        }
      }
      if (loading_stop_values.size() > 1) {
        if (const auto departure = moirai::find_string_member(stop, "rel_etd");
            departure.has_value()) {
          spec.relative_departure =
              std::chrono::duration_cast<TIME_OF_DAY>(
                  time_string_to_time(*departure));
        }
      }
    }
    loading_stops.push_back(spec);
  }

  std::vector<RouteEdgeSpec> specs;
  specs.reserve((loading_stops.size() * (loading_stops.size() - 1U)) / 2U);
  for (std::size_t i = 0; i < loading_stops.size(); ++i) {
    for (std::size_t j = i + 1; j < loading_stops.size(); ++j) {
      const auto& source = loading_stops[i];
      const auto& target = loading_stops[j];
      const bool is_terminal = j + 1 == loading_stops.size();

      if (!source.relative_departure.has_value() ||
          !target.relative_arrival.has_value() ||
          !target.relative_departure.has_value() ||
          !source.center_code.has_value() || !target.center_code.has_value()) {
        continue;
      }

      const auto departure_as_time =
          reporting_offset + *source.relative_departure;
      const auto duration = std::chrono::duration_cast<TIME_OF_DAY>(
          *target.relative_arrival - *source.relative_departure);
      const auto duration_loading = TIME_OF_DAY{0};
      const auto duration_unloading = std::chrono::duration_cast<TIME_OF_DAY>(
          *target.relative_departure - *target.relative_arrival);

      if (duration.count() < 0) {
        continue;
      }

      specs.push_back(RouteEdgeSpec{
          .source_center_code = std::string(*source.center_code),
          .target_center_code = std::string(*target.center_code),
          .edge = TransportEdge(
              std::format("{}.{}",
                          *uuid,
                          (i * (loading_stops.size() - 1)) + j - i - 1),
              std::string(*name), departure_as_time, duration,
              duration_loading, duration_unloading,
              route_type == "air" ? VehicleType::AIR : VehicleType::SURFACE,
              route_type == "carting" ? MovementType::CARTING
                                      : MovementType::LINEHAUL,
              is_terminal, scheduled_days),
      });
    }
  }

  return specs;
}
