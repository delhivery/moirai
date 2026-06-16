export module moirai.transportation;

export import std;
export import moirai.date_utils;

export enum VehicleType : std::uint8_t {
  SURFACE = 0,
  AIR = 1,
};

export enum MovementType : std::uint8_t {
  CARTING = 0,
  LINEHAUL = 1,
};

export enum ProcessType : std::uint8_t {
  INBOUND = 0,
  OUTBOUND = 1,
  CUSTODY = 2,
};

export inline constexpr std::uint8_t ALL_DAYS_OF_WEEK = 0b0111'1111;

export template <MovementType, ProcessType> using Latency = DURATION;

export struct TransportCenter {
  static constexpr std::size_t LATENCY_SLOT_COUNT = 6;
  std::string code;
  std::string name;

public:
  TransportCenter() = default;

  TransportCenter(std::string center_code);

  TransportCenter(std::string center_code, std::string center_name);

  template <MovementType M, ProcessType P>
  void set_latency(Latency<M, P> latency) {
    m_latencies[latency_index(M, P)] = latency;
  }

  template <MovementType M, ProcessType P>
  auto get_latency() const -> Latency<M, P> {
    return m_latencies[latency_index(M, P)];
  }

  void set_cutoff(TIME_OF_DAY cutoff) { this->m_cutoff = cutoff; }

  auto get_cutoff() const -> TIME_OF_DAY { return m_cutoff; }

private:
  static constexpr auto latency_index(MovementType movement,
                                      ProcessType process) -> std::size_t {
    return (static_cast<std::size_t>(movement) * 3U) +
           static_cast<std::size_t>(process);
  }

  std::array<DURATION, LATENCY_SLOT_COUNT> m_latencies{};
  TIME_OF_DAY m_cutoff;
};

export struct TransportEdge {
  std::string code;
  std::string name;
  std::string route_prefix;

  TIME_OF_DAY departure{};

  DURATION duration{};
  DURATION duration_loading{};
  DURATION duration_unloading{};

  VehicleType vehicle{VehicleType::SURFACE};
  MovementType movement{MovementType::CARTING};
  std::uint8_t days_of_week{ALL_DAYS_OF_WEEK};

  bool transient{false};
  bool terminal{false};

  TransportEdge() = default;

  TransportEdge(std::string edge_code, std::string edge_name);

  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  TransportEdge(std::string edge_code, std::string edge_name,
                TIME_OF_DAY departure_time, DURATION transit_duration,
                DURATION loading_duration, DURATION unloading_duration,
                VehicleType vehicle_type, MovementType movement_type,
                bool is_terminal,
                std::uint8_t scheduled_days = ALL_DAYS_OF_WEEK);

  template <PathTraversalMode M> [[nodiscard]] auto weight() const -> COST;

  template <PathTraversalMode M>
  [[nodiscard]] auto weight_alt(CLOCK) const -> COST;

  [[nodiscard]] auto wgt() const -> int;

  void update(const TransportCenter& source, const TransportCenter& target);

  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  void update(const std::shared_ptr<TransportCenter>& source,
              const std::shared_ptr<TransportCenter>& target);

private:
  DURATION m_offset_source{};
  DURATION m_offset_target{};
};
