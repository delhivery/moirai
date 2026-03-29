#include "transportation.hxx"
#include <cassert>
#include <chrono> // for operator+, __duration_common_type<>::type, days
#include <utility>

TransportCenter::TransportCenter(std::string center_code)
    : code(std::move(center_code)) {}

TransportEdge::TransportEdge(std::string edge_code, std::string edge_name)
    : code(std::move(edge_code)), name(std::move(edge_name)),
      vehicle(VehicleType::SURFACE), movement(MovementType::CARTING),
      transient(true), terminal(false), m_offset_source(0), m_offset_target(0) {
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
TransportEdge::TransportEdge(
    std::string edge_code, std::string edge_name, TIME_OF_DAY departure_time,
    DURATION transit_duration, DURATION loading_duration,
    DURATION unloading_duration, VehicleType vehicle_type,
    MovementType movement_type, bool is_terminal = false)
    : code(std::move(edge_code)), name(std::move(edge_name)),
      departure(departure_time), duration(transit_duration),
      duration_loading(loading_duration),
      duration_unloading(unloading_duration), vehicle(vehicle_type),
      movement(movement_type), transient(false), terminal(is_terminal) {}
// NOLINTEND(bugprone-easily-swappable-parameters)

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void TransportEdge::update(const std::shared_ptr<TransportCenter> &source,
                           const std::shared_ptr<TransportCenter> &target) {

  if (movement == MovementType::CARTING) {
    m_offset_source =
        source->get_latency<MovementType::CARTING, ProcessType::OUTBOUND>();
    m_offset_target =
        target->get_latency<MovementType::CARTING, ProcessType::INBOUND>();
  } else {
    m_offset_source =
        source->get_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>();
    m_offset_target =
        target->get_latency<MovementType::LINEHAUL, ProcessType::INBOUND>();
  }

  m_offset_source += duration_loading / 2;
  if (terminal) {
    m_offset_target += duration_unloading;
  } else {
    m_offset_target += duration_unloading / 2;
  }
}

template <>
auto TransportEdge::weight<PathTraversalMode::FORWARD>() const -> COST {
  if (transient) {
    return {TIME_OF_DAY::max(), DURATION::max()};
  }
  TIME_OF_DAY actual_departure{
      datemod(departure - m_offset_source, std::chrono::days{1})};
  return {actual_departure, m_offset_source + duration + m_offset_target};
}

template <>
auto TransportEdge::weight<PathTraversalMode::REVERSE>() const -> COST {
  if (transient) {
    return {TIME_OF_DAY::max(), DURATION::max()};
  }
  TIME_OF_DAY actual_departure{
      datemod(departure + duration + m_offset_target, std::chrono::days{1})};
  return {actual_departure, duration + m_offset_source + m_offset_target};
}
