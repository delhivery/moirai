#ifndef MOIRAI_TRANSPORTATION
#define MOIRAI_TRANSPORTATION

#include <chrono>
#include <cstdint>
#include <map>
#include <string>

#include "date_utils.hxx"

enum VehicleType : uint8_t
{
  SURFACE = 0,
  AIR = 1,
};

enum MovementType : uint8_t
{
  CARTING = 0,
  LINEHAUL = 1,
};

enum ProcessType : uint8_t
{
  INBOUND = 0,
  OUTBOUND = 1,
  CUSTODY = 2,
};

enum PathTraversalMode : uint8_t
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

  DURATION duration;

  VehicleType vehicle;
  MovementType movement;

  template <PathTraversalMode M>
  DURATION weight(CLOCK start);
};

#endif
