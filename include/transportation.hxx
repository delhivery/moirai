#ifndef MOIRAI_TRANSPORTATION
#define MOIRAI_TRANSPORTATION

#include "date_utils.hxx" // for DURATION, COST, TIME_OF_DAY
#include <cstdint>        // for uint8_t
#include <map>            // for map, map<>::mapped_type
#include <memory>         // for shared_ptr
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
using Latency = DURATION_MINUTES;

struct TransportCenter
{
  std::string m_code, m_name;

public:
  TransportCenter() = default;

  TransportCenter(const std::string&, const std::string&);

  template<MovementType M, ProcessType P>
  void set_latency(Latency<M, P> latency)
  {
    m_latencies[std::make_pair(M, P)] = latency;
  }

  template<MovementType M, ProcessType P>
  Latency<M, P> get_latency()
  {
    auto const key = std::make_pair(M, P);

    return m_latencies.contains(key) ? m_latencies[key] : Latency<M, P>(0);
  }

  void set_cutoff(TIME_OF_DAY_MINUTES cutoff) { this->m_cutoff = cutoff; }

  TIME_OF_DAY_MINUTES get_cutoff() { return m_cutoff; }

private:
  std::map<std::pair<MovementType, ProcessType>, DURATION_MINUTES> m_latencies;
  TIME_OF_DAY_MINUTES m_cutoff;
};

struct TransportEdge
{
  std::string m_code;
  std::string m_name;

  TIME_OF_DAY_MINUTES m_departure;

  DURATION_MINUTES m_duration;
  DURATION_MINUTES m_duration_loading;
  DURATION_MINUTES m_duration_unloading;

  VehicleType m_vehicle;
  MovementType m_movement;

  bool m_transient;
  bool m_terminal;

  TransportEdge()
    : m_transient(false)
  {
  }

  TransportEdge(std::string, std::string);

  TransportEdge(std::string,
                std::string,
                TIME_OF_DAY_MINUTES,
                DURATION_MINUTES,
                DURATION_MINUTES,
                DURATION_MINUTES,
                VehicleType,
                MovementType,
                bool);

  template<PathTraversalMode M>
  TemporalEdgeCostAttributes weight() const;

  const CLOCK_MINUTES& departure(const CLOCK_MINUTES&) const;

  void update(std::shared_ptr<TransportCenter>,
              std::shared_ptr<TransportCenter>);

private:
  DURATION_MINUTES m_offset_source;
  DURATION_MINUTES m_offset_target;
};

struct TransportationLoadSubItem
{
  std::string m_idx, m_target_idx;
  CLOCK_MINUTES m_reach_by;
};

#endif
