export module moirai.capacity_optimizer;

export import std;
export import moirai.date_utils;
export import moirai.json_utils;
export import moirai.solver;

export namespace moirai {

enum class CapacityUpdateMode : std::uint8_t {
  SNAPSHOT,
  DELTA,
};

struct CapacityIngestionRecord {
  CapacityInstanceKey key;
  CapacityUpdateMode mode{CapacityUpdateMode::SNAPSHOT};
  std::optional<CapacityUnits> capacity;
  std::optional<CapacityUnits> reserved;
  std::optional<CapacityUnits> consumed;
  std::optional<CapacityUnits> protected_units;
  std::optional<double> price;
  std::optional<double> reserve_ratio;
  std::optional<CapacityUnits> min_protected_units;
  std::optional<CapacityUnits> max_protected_units;
};

struct OptimizationPathCandidate {
  std::vector<CapacityInstanceKey> capacity_instances;
  CapacityUnits units{1};
  std::uint32_t hops{};
  CLOCK arrival{};
  CLOCK deadline{CLOCK::max()};
  double cost{};
  bool sla_feasible{true};
};

struct OptimizationShipment {
  std::string id;
  std::vector<OptimizationPathCandidate> candidates;
  CapacityUnits units{1};
  CLOCK deadline{CLOCK::max()};
  CapacityReservationOptions reservation_options;
  std::uint8_t priority{};
};

struct RollingHorizonConfig {
  bool enabled{false};
  bool use_highs{true};
  bool commit_reservations{false};
  std::size_t max_batch_size{64};
  std::chrono::milliseconds batch_window{0};
  double sla_breach_penalty{1'000'000.0};
  double hop_penalty{1'000.0};
  double lateness_penalty_per_minute{100.0};
  double capacity_price_scale{10.0};
  double price_smoothing{0.0};
};

struct CapacityProtectionPolicy {
  double reserve_ratio{0.0};
  CapacityUnits min_protected_units{0};
  std::optional<CapacityUnits> max_protected_units;
};

struct RollingHorizonResult {
  bool used_highs{false};
  bool optimal{false};
  std::vector<std::optional<std::size_t>> chosen_candidate;
  std::unordered_map<CapacityInstanceKey, double, CapacityInstanceKeyHash>
      capacity_prices;
};

enum class CapacityStateBackend : std::uint8_t {
  AUTO,
  SNAPSHOT,
  ROCKSDB,
};

class CapacityStateLog {
private:
  std::filesystem::path m_path;
  CapacityStateBackend m_backend{CapacityStateBackend::AUTO};
  mutable std::mutex m_mutex;

public:
  explicit CapacityStateLog(
      std::filesystem::path path,
      CapacityStateBackend backend = CapacityStateBackend::AUTO);

  [[nodiscard]] auto path() const -> const std::filesystem::path&;
  [[nodiscard]] auto backend() const -> CapacityStateBackend;
  void clear() const;
  void restore(CapacityReservationStore& store) const;
  void persist_snapshot(const CapacityReservationStore& store) const;
};

[[nodiscard]] auto parse_capacity_ingestion_record(const Json& object)
    -> std::expected<CapacityIngestionRecord, std::string>;

void apply_capacity_ingestion_record(
    CapacityReservationStore& store,
    const CapacityIngestionRecord& record,
    CapacityProtectionPolicy protection_policy = {});

class RollingHorizonOptimizer {
private:
  RollingHorizonConfig m_config;

public:
  explicit RollingHorizonOptimizer(RollingHorizonConfig config = {});

  [[nodiscard]] auto optimize(
      std::span<const OptimizationShipment> shipments,
      const CapacityReservationStore& capacity) const -> RollingHorizonResult;

  [[nodiscard]] auto commit(
      std::span<const OptimizationShipment> shipments,
      const RollingHorizonResult& result,
      CapacityReservationStore& capacity) const -> std::size_t;
};

} // namespace moirai
