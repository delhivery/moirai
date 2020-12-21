#include "transportation.hxx"
#include <chrono> // for operator+, __duration_common_type<>::type, days
#include <ratio>  // for ratio

TransportCenter::TransportCenter(std::string code, std::string name)
  : code(code)
  , name(name)
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
{}

void
TransportEdge::update(TransportCenter source, TransportCenter target)
{

  if (movement == MovementType::CARTING) {
    offset_source =
      source.get_latency<MovementType::CARTING, ProcessType::OUTBOUND>();
    offset_target =
      target.get_latency<MovementType::CARTING, ProcessType::INBOUND>();
  } else {
    offset_source =
      source.get_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>();
    offset_target =
      target.get_latency<MovementType::LINEHAUL, ProcessType::INBOUND>();
  }
}

template<>
COST
TransportEdge::weight<PathTraversalMode::FORWARD>() const
{
  TIME_OF_DAY actual_departure{ (departure - offset_source).count() %
                                std::chrono::days{ 1 }.count() };
  return { actual_departure, offset_source + duration };
}

template<>
COST
TransportEdge::weight<PathTraversalMode::REVERSE>() const
{
  TIME_OF_DAY actual_departure{ (departure + duration + offset_target).count() %
                                std::chrono::days{ 1 }.count() };
  return { actual_departure, duration + offset_source + offset_target };
}
