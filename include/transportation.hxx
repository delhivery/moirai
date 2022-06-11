#ifndef MOIRAI_TRANSPORTATION
#define MOIRAI_TRANSPORTATION

#include "date_utils.hxx" // for DURATION, COST, TIME_OF_DAY
#include "edge_cost_attributes.hxx"
#include <cstdint> // for uint8_t
#include <map>     // for map, map<>::mapped_type
#include <memory>  // for shared_ptr
#include <string>  // for string
#include <utility> // for pair, make_pair

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

template<MovementType, ProcessType>
using Latency = minutes;

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

  void set_cutoff(time_of_day cutoff) { this->m_cutoff = cutoff; }

  time_of_day get_cutoff() { return m_cutoff; }

private:
  std::map<std::pair<MovementType, ProcessType>, minutes> m_latencies;
  time_of_day m_cutoff;
};

struct TransportEdge
{
  std::string m_code;
  std::string m_name;

  time_of_day m_departure;

  minutes m_duration;
  minutes m_duration_loading;
  minutes m_duration_unloading;

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
                time_of_day,
                minutes,
                minutes,
                minutes,
                VehicleType,
                MovementType,
                bool);

  template<PathTraversalMode M>
  TemporalEdgeCostAttributes weight() const;

  const datetime& departure(const datetime&) const;

  void update(std::shared_ptr<TransportCenter>,
              std::shared_ptr<TransportCenter>);

private:
  minutes m_offset_source;
  minutes m_offset_target;
};

struct TransportationLoadSubItem
{
  std::string m_idx, m_target_idx;
  datetime m_reach_by;
};

#endif
