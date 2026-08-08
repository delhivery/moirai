export module moirai.solver;

export import std;
import moirai.date_utils;
import moirai.transportation;

export using NodeId = std::uint32_t;
export using EdgeId = std::uint32_t;
export using SolverMinute = std::uint32_t;
export using SolverScore = std::int64_t;
export using CapacityUnits = std::uint32_t;
export inline constexpr NodeId INVALID_NODE =
    std::numeric_limits<NodeId>::max();
export inline constexpr EdgeId INVALID_EDGE =
    std::numeric_limits<EdgeId>::max();

export struct CapacityInstanceKey {
  EdgeId edge{INVALID_EDGE};
  SolverMinute departure{};

  [[nodiscard]] auto operator==(const CapacityInstanceKey&) const
      -> bool = default;
};

export struct CapacityInstanceKeyHash {
  [[nodiscard]] auto operator()(const CapacityInstanceKey& key) const noexcept
      -> std::size_t {
    const auto edge_hash = std::hash<EdgeId>{}(key.edge);
    const auto departure_hash = std::hash<SolverMinute>{}(key.departure);
    return edge_hash ^ (departure_hash + 0x9e3779b97f4a7c15ULL +
                        (edge_hash << 6U) + (edge_hash >> 2U));
  }
};

export struct CapacityBucket {
  CapacityUnits capacity{};
  CapacityUnits reserved{};
  CapacityUnits consumed{};
  CapacityUnits protected_units{};
  double price{};
};

export struct CapacityReservation {
  std::vector<CapacityInstanceKey> instances;
  CapacityUnits units{1};
  CLOCK reserved_at{};
  CLOCK expires_at{CLOCK::max()};
  CLOCK frozen_at{CLOCK::max()};
  std::string payload;

  [[nodiscard]] auto empty() const -> bool { return instances.empty(); }
};

export struct CapacityReservationOptions {
  CLOCK reserved_at{};
  CLOCK expires_at{CLOCK::max()};
  CLOCK frozen_at{CLOCK::max()};
  std::string payload;
};

export struct CapacityStoreMetrics {
  std::uint64_t reservations_created{};
  std::uint64_t reservations_replaced{};
  std::uint64_t reservations_released{};
  std::uint64_t reservations_consumed{};
  std::uint64_t reservations_expired{};
  std::uint64_t reservation_failures{};
  std::uint64_t invalid_buckets_clamped{};
  std::uint64_t protected_capacity_saved{};
  std::uint64_t protected_capacity_used{};
};

export class CapacityReservationStore {
private:
  mutable std::mutex m_mutex;
  std::unordered_map<CapacityInstanceKey, CapacityBucket, CapacityInstanceKeyHash>
      m_buckets;
	  std::unordered_map<std::string, CapacityReservation> m_reservations;
	  std::optional<CapacityUnits> m_default_capacity;
	  mutable CapacityStoreMetrics m_metrics;
	  bool m_enabled{false};
  bool m_missing_capacity_allowed{true};

public:
  void set_enabled(bool enabled);
  [[nodiscard]] auto enabled() const -> bool;

  void set_missing_capacity_allowed(bool allowed);
  [[nodiscard]] auto missing_capacity_allowed() const -> bool;

  void set_default_capacity(std::optional<CapacityUnits> capacity);
  [[nodiscard]] auto default_capacity() const -> std::optional<CapacityUnits>;

  void set_capacity(CapacityInstanceKey key, CapacityUnits capacity);
  void set_bucket(CapacityInstanceKey key, CapacityBucket bucket);
  [[nodiscard]] auto bucket(CapacityInstanceKey key) const
      -> std::optional<CapacityBucket>;
  void set_protected_capacity(CapacityInstanceKey key, CapacityUnits units);
  [[nodiscard]] auto protected_capacity(CapacityInstanceKey key) const
      -> CapacityUnits;
  void set_price(CapacityInstanceKey key, double price);
  [[nodiscard]] auto price(CapacityInstanceKey key) const -> double;
  [[nodiscard]] auto remaining(CapacityInstanceKey key) const
      -> std::optional<CapacityUnits>;
  [[nodiscard]] auto available(CapacityInstanceKey key, CapacityUnits units,
                               bool allow_protected_capacity = false) const
      -> bool;
  [[nodiscard]] auto penalty_minutes(CapacityInstanceKey key, CapacityUnits units,
                                     bool allow_protected_capacity,
                                     double price_weight,
                                     double protection_weight) const
      -> SolverScore;
  [[nodiscard]] auto reserve(std::span<const CapacityInstanceKey> instances,
                             CapacityUnits units) -> std::optional<CapacityReservation>;
	  [[nodiscard]] auto reserve_for(std::string owner,
	                                 std::span<const CapacityInstanceKey> instances,
	                                 CapacityUnits units,
	                                 CapacityReservationOptions options = {})
	      -> std::optional<CapacityReservation>;
	  void release(const CapacityReservation& reservation);
  [[nodiscard]] auto release(std::string_view owner)
      -> std::optional<CapacityReservation>;
	  [[nodiscard]] auto consume(std::string_view owner)
	      -> std::optional<CapacityReservation>;
	  [[nodiscard]] auto expire_before(CLOCK deadline)
	      -> std::vector<std::pair<std::string, CapacityReservation>>;
	  void clear();
	  [[nodiscard]] auto reservation(std::string_view owner) const
	      -> std::optional<CapacityReservation>;
	  [[nodiscard]] auto metrics() const -> CapacityStoreMetrics;
	  [[nodiscard]] auto buckets_snapshot() const
	      -> std::vector<std::pair<CapacityInstanceKey, CapacityBucket>>;
  [[nodiscard]] auto reservations_snapshot() const
      -> std::vector<std::pair<std::string, CapacityReservation>>;
};

export struct CapacitySearch {
  const CapacityReservationStore* store{nullptr};
  std::span<const EdgeId> excluded_edges{};
  CapacityUnits units{1};
  std::uint8_t search_weeks{1};
  bool allow_protected_capacity{false};
  double price_weight{0.0};
  double protection_weight{0.0};
};

export struct PathCandidateMetrics {
  std::size_t requested{};
  std::size_t generated{};
  std::size_t expansions{};
  std::size_t duplicates{};
  std::size_t route_duplicates{};
  std::size_t hub_duplicates{};
  std::size_t departure_day_duplicates{};
  std::size_t slack_duplicates{};
  std::int64_t latency_ms{};
  bool expansion_cap_reached{false};
};

export struct PathCandidateOptions {
  std::size_t max_candidates{1};
  std::uint8_t search_weeks{1};
  std::size_t max_expansions{256};
  bool allow_protected_capacity{false};
  double price_weight{0.0};
  double protection_weight{0.0};
  CLOCK deadline{CLOCK::max()};
  bool diversify_routes{true};
  bool diversify_hubs{true};
  bool diversify_departure_days{true};
  bool diversify_slack{true};
  PathCandidateMetrics* metrics{nullptr};
};

export struct PathStep {
  const TransportCenter* node{nullptr};
  const TransportEdge* outbound{nullptr};
  EdgeId outbound_edge{INVALID_EDGE};
  CLOCK distance{};
  CLOCK departure{};
};

export struct Path {
  std::vector<PathStep> steps;

  [[nodiscard]] auto empty() const -> bool { return steps.empty(); }
  [[nodiscard]] explicit operator bool() const { return !empty(); }
  [[nodiscard]] auto front() const -> const PathStep& { return steps.front(); }
  [[nodiscard]] auto back() const -> const PathStep& { return steps.back(); }
};

export struct SolverEdgeHot {
  EdgeId id{INVALID_EDGE};
  NodeId source{INVALID_NODE};
  NodeId target{INVALID_NODE};
  EdgeId cold{INVALID_EDGE};
  std::array<SolverMinute, 7> forward_schedule{};
  std::array<SolverMinute, 7> reverse_schedule{};
  SolverMinute forward_duration{};
  SolverMinute reverse_duration{};
  SolverMinute reverse_outbound_latency{};
  std::uint8_t forward_schedule_count{};
  std::uint8_t reverse_schedule_count{};
  VehicleType vehicle{VehicleType::SURFACE};
  MovementType movement{MovementType::CARTING};
};

export struct SolverEdgeCold {
  TransportEdge edge;
};

export struct SolverGraphStats {
  std::string_view queue;
  std::size_t nodes{};
  std::size_t edges{};
  std::size_t outgoing_storage{};
  std::size_t incoming_storage{};
  double average_out_degree{};
  std::uint32_t max_out_degree{};
};

export struct TransparentStringHash {
  using is_transparent = void;

  [[nodiscard]] auto operator()(std::string_view value) const noexcept
      -> std::size_t {
    return std::hash<std::string_view>{}(value);
  }

  [[nodiscard]] auto operator()(const std::string& value) const noexcept
      -> std::size_t {
    return (*this)(std::string_view{value});
  }

  [[nodiscard]] auto operator()(const char* value) const noexcept
      -> std::size_t {
    return (*this)(std::string_view{value});
  }
};

export struct TransparentStringEqual {
  using is_transparent = void;

  [[nodiscard]] auto operator()(std::string_view lhs,
                                std::string_view rhs) const noexcept -> bool {
    return lhs == rhs;
  }
};

export class Solver {
private:
  std::vector<TransportCenter> m_nodes;
  std::vector<SolverEdgeHot> m_edges;
  std::vector<SolverEdgeCold> m_edge_details;
  mutable std::vector<EdgeId> m_outgoing_edges;
  mutable std::vector<EdgeId> m_incoming_edges;
  mutable std::vector<std::uint32_t> m_outgoing_offsets;
  mutable std::vector<std::uint32_t> m_incoming_offsets;
  mutable std::mutex m_csr_mutex;
  mutable std::atomic_bool m_csr_dirty{true};
  std::unordered_map<std::string,
                     NodeId,
                     TransparentStringHash,
                     TransparentStringEqual>
      m_node_by_name;
  std::unordered_map<std::string,
                     EdgeId,
                     TransparentStringHash,
                     TransparentStringEqual>
      m_edge_by_name;

  [[nodiscard]] auto valid_node(NodeId node) const -> bool;
  void invalidate_graph();
  void rebuild_csr() const;
  [[nodiscard]] auto outgoing_edges(NodeId node) const -> std::span<const EdgeId>;
  [[nodiscard]] auto incoming_edges(NodeId node) const -> std::span<const EdgeId>;
  [[nodiscard]] auto build_forward_path(NodeId source, NodeId target,
                                        const std::vector<SolverMinute>& distances,
                                        const std::vector<EdgeId>& predecessors,
                                        const std::vector<SolverMinute>& departures)
      const -> Path;
  [[nodiscard]] auto build_reverse_path(NodeId source, NodeId target,
                                        const std::vector<SolverMinute>& distances,
                                        const std::vector<EdgeId>& predecessors,
                                        const std::vector<SolverMinute>& departures)
      const -> Path;

  template <PathTraversalMode P, VehicleType V>
  [[nodiscard]] auto find_path_impl(NodeId source, NodeId target,
                                    CLOCK start,
                                    const CapacitySearch* capacity) const -> Path;

public:
  void finalize_graph() const;

  void reserve_nodes(std::size_t count);

  void reserve_edges(std::size_t count);

  [[nodiscard]] auto add_node(TransportCenter center) -> NodeId;

  [[nodiscard]] auto add_node(const std::shared_ptr<TransportCenter>& center)
      -> NodeId;

  [[nodiscard]] auto find_node(std::string_view node_code_or_name) const
      -> std::optional<NodeId>;

  [[nodiscard]] auto get_node(NodeId node) const
      -> std::shared_ptr<TransportCenter>;

  [[nodiscard]] auto graph_stats() const -> SolverGraphStats;

  [[nodiscard]] auto add_edge(NodeId source, NodeId target, TransportEdge route)
      -> EdgeId;

  [[nodiscard]] auto add_edge(NodeId source, NodeId target,
                              const std::shared_ptr<TransportEdge>& route)
      -> EdgeId;

  [[nodiscard]] auto find_edge(std::string_view edge_code) const
      -> std::optional<EdgeId>;

  [[nodiscard]] auto show() const -> std::string;

  [[nodiscard]] auto show_all() const -> std::string;

  template <PathTraversalMode P, VehicleType V = VehicleType::AIR>
  [[nodiscard]] auto find_path(NodeId source, NodeId target, CLOCK start) const
      -> Path;

  template <PathTraversalMode P, VehicleType V = VehicleType::AIR>
  [[nodiscard]] auto find_path(NodeId source, NodeId target, CLOCK start,
                               const CapacitySearch& capacity) const -> Path;

  template <PathTraversalMode P, VehicleType V = VehicleType::AIR>
  [[nodiscard]] auto find_path_candidates(
      NodeId source, NodeId target, CLOCK start,
      const CapacitySearch& capacity,
      const PathCandidateOptions& options = {}) const -> std::vector<Path>;
};
