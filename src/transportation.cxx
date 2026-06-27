module moirai.transportation;

import std;
import moirai.date_utils;

namespace {

auto route_prefix_from_code(const std::string& code) -> std::string {
  return code.substr(0, code.find('.'));
}

} // namespace

TransportCenter::TransportCenter(std::string center_code)
    : code(std::move(center_code)) {}

TransportCenter::TransportCenter(std::string center_code, std::string center_name)
    : code(std::move(center_code)), name(std::move(center_name)) {}

TransportEdge::TransportEdge(std::string edge_code, std::string edge_name)
    : code(std::move(edge_code)), name(std::move(edge_name)),
      route_prefix(route_prefix_from_code(code)),
      vehicle(VehicleType::SURFACE), movement(MovementType::CARTING),
      days_of_week(ALL_DAYS_OF_WEEK), transient(true), terminal(false),
      m_offset_source(0), m_offset_target(0) {
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
TransportEdge::TransportEdge(
    std::string edge_code, std::string edge_name, TIME_OF_DAY departure_time,
    DURATION transit_duration, DURATION loading_duration,
    DURATION unloading_duration, VehicleType vehicle_type,
    MovementType movement_type, bool is_terminal, std::uint8_t scheduled_days)
    : code(std::move(edge_code)), name(std::move(edge_name)),
      route_prefix(route_prefix_from_code(code)), departure(departure_time),
      duration(transit_duration),
      duration_loading(loading_duration),
      duration_unloading(unloading_duration), vehicle(vehicle_type),
      movement(movement_type), days_of_week(scheduled_days), transient(false),
      terminal(is_terminal) {}
// NOLINTEND(bugprone-easily-swappable-parameters)

void TransportEdge::update(const TransportCenter &source,
                           const TransportCenter &target) {
  (void)target;
  DURATION outbound_processing{};
  if (movement == MovementType::CARTING) {
    outbound_processing =
      source.get_latency<MovementType::CARTING, ProcessType::OUTBOUND>();
  } else {
    outbound_processing =
      source.get_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>();
  }

  m_offset_source = std::max(outbound_processing, duration_loading);
  m_offset_target = duration_unloading;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void TransportEdge::update(const std::shared_ptr<TransportCenter> &source,
                           const std::shared_ptr<TransportCenter> &target) {
  update(*source, *target);
}

template <>
auto TransportEdge::weight<PathTraversalMode::FORWARD>() const -> COST {
  if (transient) {
    return {.unreachable = true};
  }
  return {.schedule_offset = departure - m_offset_source,
          .duration = m_offset_source + duration + m_offset_target,
          .days_of_week = days_of_week};
}

template <>
auto TransportEdge::weight<PathTraversalMode::REVERSE>() const -> COST {
  if (transient) {
    return {.unreachable = true};
  }
  return {.schedule_offset = departure + duration + m_offset_target,
          .duration = duration + m_offset_source + m_offset_target,
          .days_of_week = days_of_week};
}
