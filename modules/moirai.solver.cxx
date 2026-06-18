export module moirai.solver;

export import std;
import moirai.date_utils;
import moirai.transportation;

export using NodeId = std::uint32_t;
export using EdgeId = std::uint32_t;
export using SolverMinute = std::uint32_t;
export inline constexpr NodeId INVALID_NODE =
    std::numeric_limits<NodeId>::max();
export inline constexpr EdgeId INVALID_EDGE =
    std::numeric_limits<EdgeId>::max();

export struct PathStep {
  const TransportCenter* node{nullptr};
  const TransportEdge* outbound{nullptr};
  CLOCK distance{};
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
                                        const std::vector<EdgeId>& predecessors)
      const -> Path;
  [[nodiscard]] auto build_reverse_path(NodeId source, NodeId target,
                                        const std::vector<SolverMinute>& distances,
                                        const std::vector<EdgeId>& predecessors)
      const -> Path;

  template <PathTraversalMode P, VehicleType V>
  [[nodiscard]] auto find_path_impl(NodeId source, NodeId target,
                                    CLOCK start) const -> Path;

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
};
