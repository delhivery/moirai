#pragma once

#include "date_utils.hxx" // for DURATION, COST, TIME_OF_DAY
#include <array>
#include <cstddef>
#include <cstdint> // for uint8_t
#include <memory>  // for shared_ptr
#include <string>  // for string

enum VehicleType : std::uint8_t {
  SURFACE = 0,
  AIR = 1,
};

enum MovementType : std::uint8_t {
  CARTING = 0,
  LINEHAUL = 1,
};

enum ProcessType : std::uint8_t {
  INBOUND = 0,
  OUTBOUND = 1,
  CUSTODY = 2,
};

enum PathTraversalMode : std::uint8_t {
  FORWARD = 0,
  REVERSE = 1,
};

template <MovementType, ProcessType> using Latency = DURATION;

struct TransportCenter {
  static constexpr std::size_t LATENCY_SLOT_COUNT = 6;
  std::string code;

public:
  TransportCenter() = default;

  TransportCenter(std::string center_code);

  template <MovementType M, ProcessType P>
  void set_latency(Latency<M, P> latency) {
    m_latencies[latency_index(M, P)] = latency;
  }

  template <MovementType M, ProcessType P> auto get_latency() -> Latency<M, P> {
    return m_latencies[latency_index(M, P)];
  }

  void set_cutoff(TIME_OF_DAY cutoff) { this->m_cutoff = cutoff; }

  auto get_cutoff() -> TIME_OF_DAY { return m_cutoff; }

private:
  static constexpr auto latency_index(MovementType movement,
                                      ProcessType process) -> size_t {
    return (static_cast<size_t>(movement) * 3U) + static_cast<size_t>(process);
  }

  std::array<DURATION, LATENCY_SLOT_COUNT> m_latencies{};
  TIME_OF_DAY m_cutoff;
};

struct TransportEdge {
  std::string code;
  std::string name;

  TIME_OF_DAY departure;

  DURATION duration;
  DURATION duration_loading;
  DURATION duration_unloading;

  VehicleType vehicle;
  MovementType movement;

  bool transient;
  bool terminal;

  TransportEdge() : transient(false) {}

  TransportEdge(std::string edge_code, std::string edge_name);

  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  TransportEdge(std::string edge_code, std::string edge_name,
                TIME_OF_DAY departure_time, DURATION transit_duration,
                DURATION loading_duration, DURATION unloading_duration,
                VehicleType vehicle_type, MovementType movement_type,
                bool is_terminal);

  template <PathTraversalMode M> [[nodiscard]] auto weight() const -> COST;

  template <PathTraversalMode M>
  [[nodiscard]] auto weight_alt(CLOCK) const -> COST;

  [[nodiscard]] auto wgt() const -> int;

  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  void update(const std::shared_ptr<TransportCenter> &source,
              const std::shared_ptr<TransportCenter> &target);

private:
  DURATION m_offset_source;
  DURATION m_offset_target;
};
