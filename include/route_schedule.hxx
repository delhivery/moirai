#pragma once

#include "json_utils.hxx"
#include "transportation.hxx"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct RouteEdgeSpec {
  std::string source_center_code;
  std::string target_center_code;
  std::shared_ptr<TransportEdge> edge;
};

inline auto parse_route_days_of_week(const moirai::Json& route)
    -> std::uint8_t {
  const auto* days = moirai::find_array_member(route, "days_of_week");
  if (days == nullptr || days->empty()) {
    return ALL_DAYS_OF_WEEK;
  }

  std::uint8_t mask = 0;
  for (const auto& day : *days) {
    const auto value = moirai::get_integer<int16_t>(day);
    if (!value.has_value()) {
      continue;
    }

    const auto normalized_day = ((*value % 7) + 7) % 7;
    mask |= static_cast<std::uint8_t>(1U << normalized_day);
  }

  return mask == 0 ? ALL_DAYS_OF_WEEK : mask;
}

inline auto lower_copy(std::string_view value) -> std::string {
  std::string lowered{value};
  std::ranges::transform(lowered, lowered.begin(), [](unsigned char input) {
    return static_cast<char>(std::tolower(input));
  });
  return lowered;
}

inline auto build_route_edge_specs(const moirai::Json& route,
                                   DURATION ist_offset)
    -> std::vector<RouteEdgeSpec> {
  const auto uuid = moirai::find_string_member(route, "route_schedule_uuid");
  const auto name = moirai::find_string_member(route, "name");
  const auto route_type_value = moirai::find_string_member(route, "route_type");
  const auto reporting_time = moirai::find_string_member(route, "reporting_time");
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

  std::vector<const moirai::Json*> loading_stops;
  loading_stops.reserve(stops->size());
  for (const auto& stop : *stops) {
    const auto* loading_allowed = moirai::find_member(stop, "loading_allowed");
    if (loading_allowed != nullptr && loading_allowed->is_boolean() &&
        !loading_allowed->get<bool>()) {
      continue;
    }
    loading_stops.push_back(&stop);
  }

  std::vector<RouteEdgeSpec> specs;
  for (size_t i = 0; i < loading_stops.size(); ++i) {
    for (size_t j = i + 1; j < loading_stops.size(); ++j) {
      const auto& source = *loading_stops[i];
      const auto& target = *loading_stops[j];
      const bool is_terminal = j + 1 == loading_stops.size();

      const auto arrival_in = moirai::find_string_member(source, "rel_etd");
      const auto departure = moirai::find_string_member(source, "rel_etd");
      const auto arrival = moirai::find_string_member(target, "rel_eta");
      const auto departure_out = moirai::find_string_member(target, "rel_etd");
      const auto source_center_code =
          moirai::find_string_member(source, "center_code");
      const auto target_center_code =
          moirai::find_string_member(target, "center_code");
      if (!arrival_in.has_value() || !departure.has_value() ||
          !arrival.has_value() || !departure_out.has_value() ||
          !source_center_code.has_value() || !target_center_code.has_value()) {
        continue;
      }

      const auto departure_as_time =
          reporting_offset + std::chrono::duration_cast<TIME_OF_DAY>(
                                 time_string_to_time(*departure));
      const auto duration = std::chrono::duration_cast<TIME_OF_DAY>(
          time_string_to_time(*arrival) -
          std::chrono::duration_cast<TIME_OF_DAY>(
              time_string_to_time(*departure)));
      const auto duration_loading = std::chrono::duration_cast<TIME_OF_DAY>(
          time_string_to_time(*departure) -
          std::chrono::duration_cast<TIME_OF_DAY>(
              time_string_to_time(*arrival_in)));
      const auto duration_unloading = std::chrono::duration_cast<TIME_OF_DAY>(
          time_string_to_time(*departure_out) -
          std::chrono::duration_cast<TIME_OF_DAY>(time_string_to_time(*arrival)));

      if (duration.count() < 0) {
        continue;
      }

      specs.push_back(RouteEdgeSpec{
          .source_center_code = std::string(*source_center_code),
          .target_center_code = std::string(*target_center_code),
          .edge =
              std::make_shared<TransportEdge>(
                  std::format("{}.{}",
                              *uuid,
                              (i * (loading_stops.size() - 1)) + j - i - 1),
                  std::string(*name), departure_as_time, duration,
                  duration_loading, duration_unloading,
                  route_type == "air" ? VehicleType::AIR
                                      : VehicleType::SURFACE,
                  route_type == "carting" ? MovementType::CARTING
                                          : MovementType::LINEHAUL,
                  is_terminal, scheduled_days),
      });
    }
  }

  return specs;
}
