#include "transportation.hxx"
#include <cassert>
#include <chrono>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <iostream>
#include <ratio>

TransportCenter::TransportCenter(const std::string& code,
                                 const std::string& name)
  : m_code(code)
  , m_name(name)
{
}

TransportEdge::TransportEdge(std::string code, std::string name)
  : m_code(code)
  , m_name(name)
  , m_transient(true)
  , m_vehicle(VehicleType::SURFACE)
  , m_movement(MovementType::CARTING)
  , m_offset_source(0)
  , m_offset_target(0)
  , m_terminal(false)
{
}

TransportEdge::TransportEdge(std::string code,
                             std::string name,
                             TIME_OF_DAY departure,
                             DURATION duration,
                             DURATION duration_loading,
                             DURATION duration_unloading,
                             VehicleType vehicle,
                             MovementType movement,
                             bool terminal = false)
  : m_code(code)
  , m_name(name)
  , m_departure(departure)
  , m_duration(duration)
  , m_duration_loading(duration_loading)
  , m_duration_unloading(duration_unloading)
  , m_vehicle(vehicle)
  , m_movement(movement)
  , m_terminal(terminal)
  , m_transient(false)
{
}

void
TransportEdge::update(std::shared_ptr<TransportCenter> source,
                      std::shared_ptr<TransportCenter> target)
{

  if (m_movement == MovementType::CARTING) {
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
  m_offset_source += m_duration_loading / 2;

  if (m_terminal)
    m_offset_target += m_duration_unloading;
  else
    m_offset_target += m_duration_unloading / 2;
}

template<>
COST
TransportEdge::weight<PathTraversalMode::FORWARD>() const
{
  if (m_transient)
    return { TIME_OF_DAY::max(), DURATION::max() };
  TIME_OF_DAY actual_departure{ datemod(m_departure - m_offset_source,
                                        std::chrono::days{ 1 }) };
  return { actual_departure, m_offset_source + m_duration + m_offset_target };
}

template<>
COST
TransportEdge::weight<PathTraversalMode::REVERSE>() const
{
  if (m_transient)
    return { TIME_OF_DAY::max(), DURATION::max() };
  TIME_OF_DAY actual_departure{ datemod(m_departure + m_duration + m_offset_target,
                                        std::chrono::days{ 1 }) };
  return { actual_departure, m_duration + m_offset_source + m_offset_target };
}
