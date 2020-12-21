#ifndef MOIRAI_TRANSPORTATION
#define MOIRAI_TRANSPORTATION

#include "date_utils.hxx" // for DURATION, COST, TIME_OF_DAY
#include <cstdint>        // for uint8_t
#include <map>            // for map, map<>::mapped_type
#include <string>         // for string
#include <utility>        // for pair, make_pair

enum VehicleType : std::uint8_t
{
  SURFACE = 0,
  AIR = 1,
};

enum MovementType : std::uint8_t
{
  CARTING = 0,
  LINEHAUL = 1,
};

enum ProcessType : std::uint8_t
{
  INBOUND = 0,
  OUTBOUND = 1,
  CUSTODY = 2,
};

enum PathTraversalMode : std::uint8_t
{
  FORWARD = 0,
  REVERSE = 1,
};

template<MovementType, ProcessType>
using Latency = DURATION;

struct TransportCenter
{
  std::string code;

  std::string name;

public:
  TransportCenter() = default;

  TransportCenter(std::string, std::string);

  template<MovementType M, ProcessType P>
  void set_latency(Latency<M, P> latency)
  {
    latencies[std::make_pair(M, P)] = latency;
  }

  template<MovementType M, ProcessType P>
  Latency<M, P> get_latency()
  {
    return latencies[std::make_pair(M, P)];
  }

private:
  std::map<std::pair<MovementType, ProcessType>, DURATION> latencies;
};

struct TransportEdge
{
  std::string code;
  std::string name;

  TIME_OF_DAY departure;

  DURATION duration;

  VehicleType vehicle;
  MovementType movement;

  TransportEdge() = default;

  TransportEdge(std::string, std::string, TIME_OF_DAY, DURATION, VehicleType, MovementType);

  template<PathTraversalMode M>
  COST weight() const;

  int wgt() const;

  void update(TransportCenter, TransportCenter);

private:
  DURATION offset_source;
  DURATION offset_target;
};

#endif
