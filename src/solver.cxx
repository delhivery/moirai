module moirai.solver;

import std;
import moirai.date_utils;
import moirai.transportation;

namespace {

constexpr auto MINUTES_PER_DAY = 24U * 60U;
constexpr auto DAYS_PER_WEEK = 7U;
constexpr auto MINUTES_PER_WEEK = DAYS_PER_WEEK * MINUTES_PER_DAY;
constexpr auto UNIX_EPOCH_WEEKDAY = 4U;

struct HeapEntry {
  SolverMinute distance{};
  NodeId node{INVALID_NODE};
};

struct SolverScratch {
  std::vector<SolverMinute> distances;
  std::vector<EdgeId> predecessors;
  std::vector<std::uint32_t> generations;
  std::vector<HeapEntry> heap;
#ifdef MOIRAI_SOLVER_QUEUE_BUCKET
  std::map<SolverMinute, std::vector<NodeId>, std::less<>> forward_buckets;
  std::map<SolverMinute, std::vector<NodeId>, std::greater<>> reverse_buckets;
#endif
  std::vector<NodeId> path_nodes;
  std::vector<EdgeId> path_edges;
  std::uint32_t generation{0};
  SolverMinute initial_distance{};

  void begin(std::size_t node_count, SolverMinute initial) {
    initial_distance = initial;
    if (distances.size() < node_count) {
      distances.resize(node_count);
      predecessors.resize(node_count, INVALID_EDGE);
      generations.resize(node_count, 0U);
    }
    ++generation;
    if (generation == 0U) {
      std::ranges::fill(generations, 0U);
      generation = 1U;
    }
    heap.clear();
#ifdef MOIRAI_SOLVER_QUEUE_BUCKET
    forward_buckets.clear();
    reverse_buckets.clear();
#endif
  }

  [[nodiscard]] auto distance(NodeId node) const -> SolverMinute {
    return generations[node] == generation ? distances[node] : initial_distance;
  }

  void set(NodeId node, SolverMinute distance_value, EdgeId predecessor_value) {
    generations[node] = generation;
    distances[node] = distance_value;
    predecessors[node] = predecessor_value;
  }
};

thread_local SolverScratch scratch;

[[nodiscard]] auto clock_to_minute(CLOCK value) -> SolverMinute {
  return static_cast<SolverMinute>(value.time_since_epoch().count());
}

[[nodiscard]] auto minute_to_clock(SolverMinute value) -> CLOCK {
  return CLOCK{CLOCK::duration{value}};
}

[[nodiscard]] auto positive_mod(std::int64_t lhs, std::int64_t rhs)
    -> SolverMinute {
  return static_cast<SolverMinute>(((lhs % rhs) + rhs) % rhs);
}

[[nodiscard]] auto minute_of_week(SolverMinute value) -> SolverMinute {
  const auto day = value / MINUTES_PER_DAY;
  const auto minute_of_day = value % MINUTES_PER_DAY;
  return (((day + UNIX_EPOCH_WEEKDAY) % DAYS_PER_WEEK) * MINUTES_PER_DAY) +
         minute_of_day;
}

[[nodiscard]] auto duration_to_minutes(DURATION value) -> SolverMinute {
  return static_cast<SolverMinute>(std::max<std::int64_t>(0, value.count()));
}

[[nodiscard]] auto build_weekly_schedule(const COST& cost)
    -> std::inplace_vector<SolverMinute, DAYS_PER_WEEK> {
  std::inplace_vector<SolverMinute, DAYS_PER_WEEK> schedule;
  if (cost.unreachable || cost.days_of_week == 0) {
    return schedule;
  }

  for (std::uint8_t day = 0; day < DAYS_PER_WEEK; ++day) {
    if ((cost.days_of_week & (1U << day)) == 0) {
      continue;
    }
    schedule.push_back(positive_mod(
        (static_cast<std::int64_t>(day) * MINUTES_PER_DAY) +
            cost.schedule_offset.count(),
        MINUTES_PER_WEEK));
  }
  for (std::size_t index = 1; index < schedule.size(); ++index) {
    const auto value = schedule[index];
    auto insert_at = index;
    while (insert_at > 0 && schedule[insert_at - 1U] > value) {
      schedule[insert_at] = schedule[insert_at - 1U];
      --insert_at;
    }
    schedule[insert_at] = value;
  }
  return schedule;
}

template <PathTraversalMode P>
[[nodiscard]] auto traverse(SolverMinute start, const SolverEdgeHot& edge)
    -> SolverMinute {
  const auto start_week = minute_of_week(start);
  if constexpr (P == PathTraversalMode::FORWARD) {
    if (edge.forward_schedule_count == 0) {
      return start;
    }
    const auto begin = edge.forward_schedule.begin();
    const auto end = begin + edge.forward_schedule_count;
    const auto found = std::lower_bound(begin, end, start_week);
    const SolverMinute wait =
        found != end ? *found - start_week
                     : (MINUTES_PER_WEEK - start_week) + edge.forward_schedule[0];
    return start + wait + edge.forward_duration;
  } else {
    if (edge.reverse_schedule_count == 0) {
      return start;
    }
    const auto begin = edge.reverse_schedule.begin();
    const auto end = begin + edge.reverse_schedule_count;
    const auto found = std::upper_bound(begin, end, start_week);
    const SolverMinute scheduled =
        found == begin ? edge.reverse_schedule[edge.reverse_schedule_count - 1U]
                       : *(found - 1);
    const SolverMinute wait =
        scheduled <= start_week ? start_week - scheduled
                                : start_week + MINUTES_PER_WEEK - scheduled;
    const auto next = static_cast<std::int64_t>(start) -
                      static_cast<std::int64_t>(wait) -
                      static_cast<std::int64_t>(edge.reverse_duration);
    return static_cast<SolverMinute>(std::max<std::int64_t>(0, next));
  }
}

template <PathTraversalMode P>
struct HeapCompare {
  auto operator()(const HeapEntry& lhs, const HeapEntry& rhs) const -> bool {
    if constexpr (P == PathTraversalMode::FORWARD) {
      return lhs.distance > rhs.distance;
    } else {
      return lhs.distance < rhs.distance;
    }
  }
};

template <PathTraversalMode>
[[nodiscard]] auto queue_name() -> std::string_view {
#ifdef MOIRAI_SOLVER_QUEUE_BUCKET
  return "bucket";
#else
  return "binary";
#endif
}

template <PathTraversalMode P>
void queue_push(HeapEntry entry) {
#ifdef MOIRAI_SOLVER_QUEUE_BUCKET
  if constexpr (P == PathTraversalMode::FORWARD) {
    scratch.forward_buckets[entry.distance].push_back(entry.node);
  } else {
    scratch.reverse_buckets[entry.distance].push_back(entry.node);
  }
#else
  scratch.heap.push_back(entry);
  std::push_heap(scratch.heap.begin(), scratch.heap.end(), HeapCompare<P>{});
#endif
}

template <PathTraversalMode P>
[[nodiscard]] auto queue_empty() -> bool {
#ifdef MOIRAI_SOLVER_QUEUE_BUCKET
  if constexpr (P == PathTraversalMode::FORWARD) {
    return scratch.forward_buckets.empty();
  } else {
    return scratch.reverse_buckets.empty();
  }
#else
  return scratch.heap.empty();
#endif
}

template <PathTraversalMode P>
[[nodiscard]] auto queue_pop() -> HeapEntry {
#ifdef MOIRAI_SOLVER_QUEUE_BUCKET
  if constexpr (P == PathTraversalMode::FORWARD) {
    auto bucket = scratch.forward_buckets.begin();
    const auto node = bucket->second.back();
    const auto distance = bucket->first;
    bucket->second.pop_back();
    if (bucket->second.empty()) {
      scratch.forward_buckets.erase(bucket);
    }
    return {.distance = distance, .node = node};
  } else {
    auto bucket = scratch.reverse_buckets.begin();
    const auto node = bucket->second.back();
    const auto distance = bucket->first;
    bucket->second.pop_back();
    if (bucket->second.empty()) {
      scratch.reverse_buckets.erase(bucket);
    }
    return {.distance = distance, .node = node};
  }
#else
  std::pop_heap(scratch.heap.begin(), scratch.heap.end(), HeapCompare<P>{});
  const auto current = scratch.heap.back();
  scratch.heap.pop_back();
  return current;
#endif
}

template <VehicleType V>
auto vehicle_allowed(const SolverEdgeHot& edge) -> bool {
  return edge.vehicle <= V;
}

} // namespace

auto Solver::valid_node(const NodeId node) const -> bool {
  return node < m_nodes.size();
}

void Solver::invalidate_graph() {
  m_csr_dirty = true;
}

void Solver::rebuild_csr() const {
  if (!m_csr_dirty) {
    return;
  }

  m_outgoing_offsets.assign(m_nodes.size() + 1U, 0U);
  m_incoming_offsets.assign(m_nodes.size() + 1U, 0U);
  for (const auto& edge : m_edges) {
    ++m_outgoing_offsets[static_cast<std::size_t>(edge.source) + 1U];
    ++m_incoming_offsets[static_cast<std::size_t>(edge.target) + 1U];
  }

  for (std::size_t index = 1; index < m_outgoing_offsets.size(); ++index) {
    m_outgoing_offsets[index] += m_outgoing_offsets[index - 1U];
    m_incoming_offsets[index] += m_incoming_offsets[index - 1U];
  }

  m_outgoing_edges.assign(m_edges.size(), INVALID_EDGE);
  m_incoming_edges.assign(m_edges.size(), INVALID_EDGE);
  auto outgoing_cursor = m_outgoing_offsets;
  auto incoming_cursor = m_incoming_offsets;
  for (const auto& edge : m_edges) {
    m_outgoing_edges[outgoing_cursor[edge.source]++] = edge.id;
    m_incoming_edges[incoming_cursor[edge.target]++] = edge.id;
  }

  m_csr_dirty = false;
}

void Solver::finalize_graph() const {
  rebuild_csr();
}

auto Solver::outgoing_edges(const NodeId node) const -> std::span<const EdgeId> {
  rebuild_csr();
  const auto begin = m_outgoing_offsets[node];
  const auto end = m_outgoing_offsets[static_cast<std::size_t>(node) + 1U];
  if (begin == end) {
    return {};
  }
  return std::span<const EdgeId>{m_outgoing_edges.data() + begin, end - begin};
}

auto Solver::incoming_edges(const NodeId node) const -> std::span<const EdgeId> {
  rebuild_csr();
  const auto begin = m_incoming_offsets[node];
  const auto end = m_incoming_offsets[static_cast<std::size_t>(node) + 1U];
  if (begin == end) {
    return {};
  }
  return std::span<const EdgeId>{m_incoming_edges.data() + begin, end - begin};
}

void Solver::reserve_nodes(std::size_t count) {
  m_nodes.reserve(count);
  m_outgoing_offsets.reserve(count + 1U);
  m_incoming_offsets.reserve(count + 1U);
  m_node_by_name.reserve(count);
}

void Solver::reserve_edges(std::size_t count) {
  m_edges.reserve(count);
  m_edge_details.reserve(count);
  m_outgoing_edges.reserve(count);
  m_incoming_edges.reserve(count);
  m_edge_by_name.reserve(count);
}

auto Solver::add_node(TransportCenter center) -> NodeId {
  if (const auto found = m_node_by_name.find(center.code);
      found != m_node_by_name.end()) {
    return found->second;
  }

  const auto node = static_cast<NodeId>(m_nodes.size());
  m_node_by_name[center.code] = node;
  m_nodes.push_back(std::move(center));
  invalidate_graph();
  return node;
}

auto Solver::add_node(const std::shared_ptr<TransportCenter>& center)
    -> NodeId {
  if (center == nullptr) {
    return INVALID_NODE;
  }
  return add_node(*center);
}

auto Solver::find_node(std::string_view node_code_or_name) const
    -> std::optional<NodeId> {
  if (const auto found = m_node_by_name.find(node_code_or_name);
      found != m_node_by_name.end()) {
    return found->second;
  }
  return std::nullopt;
}

auto Solver::get_node(const NodeId node) const
    -> std::shared_ptr<TransportCenter> {
  if (!valid_node(node)) {
    return nullptr;
  }
  return std::shared_ptr<TransportCenter>(
      const_cast<TransportCenter*>(&m_nodes[node]),
      [](TransportCenter*) {});
}

auto Solver::graph_stats() const -> SolverGraphStats {
  rebuild_csr();
  std::uint32_t max_degree = 0;
  for (std::size_t node = 0; node < m_nodes.size(); ++node) {
    const auto degree = m_outgoing_offsets[node + 1U] - m_outgoing_offsets[node];
    max_degree = std::max(max_degree, degree);
  }
  return SolverGraphStats{
      .queue = queue_name<PathTraversalMode::FORWARD>(),
      .nodes = m_nodes.size(),
      .edges = m_edges.size(),
      .outgoing_storage = m_outgoing_edges.size(),
      .incoming_storage = m_incoming_edges.size(),
      .average_out_degree =
          m_nodes.empty()
              ? 0.0
              : static_cast<double>(m_outgoing_edges.size()) /
                    static_cast<double>(m_nodes.size()),
      .max_out_degree = max_degree,
  };
}

auto Solver::add_edge(const NodeId source, const NodeId target,
                      TransportEdge route) -> EdgeId {
  if (!valid_node(source) || !valid_node(target)) {
    return INVALID_EDGE;
  }
  if (const auto found = m_edge_by_name.find(route.code);
      found != m_edge_by_name.end()) {
    return found->second;
  }

  route.update(m_nodes[source], m_nodes[target]);
  const auto forward_cost = route.weight<PathTraversalMode::FORWARD>();
  const auto reverse_cost = route.weight<PathTraversalMode::REVERSE>();
  const auto forward_schedule = build_weekly_schedule(forward_cost);
  const auto reverse_schedule = build_weekly_schedule(reverse_cost);
  std::array<SolverMinute, DAYS_PER_WEEK> forward_schedule_values{};
  std::array<SolverMinute, DAYS_PER_WEEK> reverse_schedule_values{};
  std::ranges::copy(forward_schedule, forward_schedule_values.begin());
  std::ranges::copy(reverse_schedule, reverse_schedule_values.begin());
  const auto reverse_outbound_latency =
      route.movement == MovementType::CARTING
          ? m_nodes[source]
                .get_latency<MovementType::CARTING, ProcessType::OUTBOUND>()
          : m_nodes[source]
                .get_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>();
  const auto edge_id = static_cast<EdgeId>(m_edges.size());
  const auto vehicle = route.vehicle;
  const auto movement = route.movement;
  m_edge_details.push_back(SolverEdgeCold{.edge = std::move(route)});
  m_edges.push_back(SolverEdgeHot{
      .id = edge_id,
      .source = source,
      .target = target,
      .cold = edge_id,
      .forward_schedule = forward_schedule_values,
      .reverse_schedule = reverse_schedule_values,
      .forward_duration = duration_to_minutes(forward_cost.duration),
      .reverse_duration = duration_to_minutes(reverse_cost.duration),
      .reverse_outbound_latency = duration_to_minutes(reverse_outbound_latency),
      .forward_schedule_count =
          static_cast<std::uint8_t>(forward_schedule.size()),
      .reverse_schedule_count =
          static_cast<std::uint8_t>(reverse_schedule.size()),
      .vehicle = vehicle,
      .movement = movement,
  });
  m_edge_by_name[m_edge_details.back().edge.code] = edge_id;
  invalidate_graph();
  return edge_id;
}

auto Solver::add_edge(const NodeId source, const NodeId target,
                      const std::shared_ptr<TransportEdge>& route) -> EdgeId {
  if (route == nullptr) {
    return INVALID_EDGE;
  }
  if (valid_node(source) && valid_node(target)) {
    route->update(m_nodes[source], m_nodes[target]);
  }
  return add_edge(source, target, *route);
}

auto Solver::find_edge(std::string_view edge_code) const
    -> std::optional<EdgeId> {
  if (const auto found = m_edge_by_name.find(edge_code);
      found != m_edge_by_name.end()) {
    return found->second;
  }
  return std::nullopt;
}

auto Solver::build_forward_path(
    const NodeId source, const NodeId target,
    const std::vector<SolverMinute>& distances,
    const std::vector<EdgeId>& predecessors) const -> Path {
  scratch.path_nodes.clear();
  scratch.path_edges.clear();

  for (NodeId current = target; current != source;) {
    scratch.path_nodes.push_back(current);
    const EdgeId predecessor = predecessors[current];
    if (predecessor == INVALID_EDGE) {
      return {};
    }

    const auto& edge = m_edges[predecessor];
    scratch.path_edges.push_back(predecessor);
    current = edge.source;
  }
  scratch.path_nodes.push_back(source);

  std::ranges::reverse(scratch.path_nodes);
  std::ranges::reverse(scratch.path_edges);

  Path path;
  path.steps.reserve(scratch.path_nodes.size());
  for (std::size_t index = 0; index < scratch.path_nodes.size(); ++index) {
    const auto node = scratch.path_nodes[index];
    const auto* outbound =
        index < scratch.path_edges.size()
            ? &m_edge_details[m_edges[scratch.path_edges[index]].cold].edge
            : nullptr;
    path.steps.push_back(PathStep{
        .node = &m_nodes[node],
        .outbound = outbound,
        .distance = minute_to_clock(distances[node]),
    });
  }
  return path;
}

auto Solver::build_reverse_path(
    const NodeId source, const NodeId target,
    const std::vector<SolverMinute>& distances,
    const std::vector<EdgeId>& predecessors) const -> Path {
  Path path;
  path.steps.reserve(scratch.path_nodes.capacity());

  for (NodeId current = target; current != source;) {
    const EdgeId predecessor = predecessors[current];
    if (predecessor == INVALID_EDGE) {
      return {};
    }

    const auto& edge = m_edges[predecessor];
    const auto distance = distances[current] + edge.reverse_outbound_latency;

    path.steps.push_back(PathStep{
        .node = &m_nodes[current],
        .outbound = &m_edge_details[edge.cold].edge,
        .distance = minute_to_clock(distance),
    });
    current = edge.target;
  }

  path.steps.push_back(PathStep{
      .node = &m_nodes[source],
      .outbound = nullptr,
      .distance = minute_to_clock(distances[source]),
  });
  return path;
}

template <PathTraversalMode P, VehicleType V>
auto Solver::find_path_impl(const NodeId source, const NodeId target,
                            CLOCK start) const -> Path {
  if (!valid_node(source) || !valid_node(target)) {
    return {};
  }

  if constexpr (P == PathTraversalMode::FORWARD) {
    scratch.begin(m_nodes.size(), std::numeric_limits<SolverMinute>::max());
  } else {
    scratch.begin(m_nodes.size(), 0U);
  }

  const auto start_minute = clock_to_minute(start);
  scratch.set(source, start_minute, INVALID_EDGE);
  queue_push<P>({.distance = start_minute, .node = source});

  while (!queue_empty<P>()) {
    const auto current = queue_pop<P>();
    if (current.distance != scratch.distance(current.node)) {
      continue;
    }
    if (current.node == target) {
      if constexpr (P == PathTraversalMode::FORWARD) {
        return build_forward_path(source, target, scratch.distances,
                                  scratch.predecessors);
      } else {
        return build_reverse_path(source, target, scratch.distances,
                                  scratch.predecessors);
      }
    }

    const auto edges = [&]() -> std::span<const EdgeId> {
      if constexpr (P == PathTraversalMode::FORWARD) {
        return outgoing_edges(current.node);
      } else {
        return incoming_edges(current.node);
      }
    }();

    for (const EdgeId edge_id : edges) {
      const auto& edge = m_edges[edge_id];
      if (!vehicle_allowed<V>(edge)) {
        continue;
      }

      const auto next_node = [&] {
        if constexpr (P == PathTraversalMode::FORWARD) {
          return edge.target;
        } else {
          return edge.source;
        }
      }();
      const SolverMinute next = traverse<P>(current.distance, edge);

      if constexpr (P == PathTraversalMode::FORWARD) {
        if (next >= scratch.distance(next_node)) {
          continue;
        }
      } else {
        if (next <= scratch.distance(next_node)) {
          continue;
        }
      }

      scratch.set(next_node, next, edge_id);
      queue_push<P>({.distance = next, .node = next_node});
    }
  }

  return {};
}

template <>
auto Solver::find_path<PathTraversalMode::FORWARD, VehicleType::AIR>(
    const NodeId source, const NodeId target, CLOCK start) const
    -> Path {
  return find_path_impl<PathTraversalMode::FORWARD, VehicleType::AIR>(
      source, target, start);
}

template <>
auto Solver::find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
    const NodeId source, const NodeId target, CLOCK start) const
    -> Path {
  return find_path_impl<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
      source, target, start);
}

template <>
auto Solver::find_path<PathTraversalMode::REVERSE, VehicleType::AIR>(
    const NodeId source, const NodeId target, CLOCK start) const
    -> Path {
  return find_path_impl<PathTraversalMode::REVERSE, VehicleType::AIR>(
      source, target, start);
}

template <>
auto Solver::find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
    const NodeId source, const NodeId target, CLOCK start) const
    -> Path {
  return find_path_impl<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
      source, target, start);
}

auto Solver::show() const -> std::string {
  return std::format("Graph<{}, {}>", m_nodes.size(), m_edges.size());
}

auto Solver::show_all() const -> std::string {
  std::vector<std::string> output;
  output.reserve(m_nodes.size() + m_edges.size());

  for (const auto& node : m_nodes) {
    output.push_back(node.code);
  }

  for (const auto& edge : m_edges) {
    output.push_back(std::format("{}: {} TO {}", m_edge_details[edge.cold].edge.code,
                                 m_nodes[edge.source].code,
                                 m_nodes[edge.target].code));
  }

  return std::accumulate(
      output.begin(), output.end(), std::string{},
      [](const std::string& acc, const std::string& arg) -> std::string {
        return std::format("{}\n{}", acc, arg);
      });
}
