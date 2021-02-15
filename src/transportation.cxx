#include "transportation.hxx"
#include <cassert>
#include <chrono> // for operator+, __duration_common_type<>::type, days
#include <fmt/core.h>
#include <iostream>
#include <ratio> // for ratio

TransportCenter::TransportCenter(std::string code)
  : code(code)
{}

TransportEdge::TransportEdge(std::string code, std::string name)
  : code(code)
  , name(name)
  , transient(true)
  , vehicle(VehicleType::SURFACE)
  , movement(MovementType::CARTING)
  , offset_source(0)
  , offset_target(0)
{}

TransportEdge::TransportEdge(std::string code,
                             std::string name,
                             TIME_OF_DAY departure,
                             DURATION duration,
                             VehicleType vehicle,
                             MovementType movement)
  : code(code)
  , name(name)
  , departure(departure)
  , duration(duration)
  , vehicle(vehicle)
  , movement(movement)
  , transient(false)
{}

void
TransportEdge::update(std::shared_ptr<TransportCenter> source,
                      std::shared_ptr<TransportCenter> target)
{

  if (movement == MovementType::CARTING) {
    offset_source =
      source->get_latency<MovementType::CARTING, ProcessType::OUTBOUND>();
    offset_target =
      target->get_latency<MovementType::CARTING, ProcessType::INBOUND>();
  } else {
    offset_source =
      source->get_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>();
    offset_target =
      target->get_latency<MovementType::LINEHAUL, ProcessType::INBOUND>();
  }
}

template<>
COST
TransportEdge::weight<PathTraversalMode::FORWARD>() const
{
  if (transient)
    return { TIME_OF_DAY::min(), DURATION::max() };
  TIME_OF_DAY actual_departure{ datemod(departure - offset_source,
                                        std::chrono::days{ 1 }) };
  return { actual_departure, offset_source + duration + offset_target };
}

template<>
COST
TransportEdge::weight<PathTraversalMode::REVERSE>() const
{
  TIME_OF_DAY actual_departure{ datemod(departure + duration + offset_target,
                                        std::chrono::days{ 1 }) };
  return { actual_departure, duration + offset_source + offset_target };
}
