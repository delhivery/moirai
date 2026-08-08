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
  SolverScore priority{};
  SolverMinute distance{};
  NodeId node{INVALID_NODE};
};

struct SolverScratch {
  std::vector<SolverMinute> distances;
  std::vector<SolverScore> priorities;
  std::vector<EdgeId> predecessors;
  std::vector<SolverMinute> departures;
  std::vector<std::uint32_t> generations;
  std::vector<HeapEntry> heap;
#ifdef MOIRAI_SOLVER_QUEUE_BUCKET
  std::map<SolverScore, std::vector<std::pair<NodeId, SolverMinute>>, std::less<>>
      forward_buckets;
  std::map<SolverScore, std::vector<std::pair<NodeId, SolverMinute>>, std::greater<>>
      reverse_buckets;
#endif
  std::vector<NodeId> path_nodes;
  std::vector<EdgeId> path_edges;
  std::uint32_t generation{0};
  SolverMinute initial_distance{};

  void begin(std::size_t node_count, SolverMinute initial) {
    initial_distance = initial;
    if (distances.size() < node_count) {
      distances.resize(node_count);
      priorities.resize(node_count);
      predecessors.resize(node_count, INVALID_EDGE);
      departures.resize(node_count, 0U);
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

  [[nodiscard]] auto priority(NodeId node) const -> SolverScore {
    if (generations[node] == generation) {
      return priorities[node];
    }
    return initial_distance == std::numeric_limits<SolverMinute>::max()
             ? std::numeric_limits<SolverScore>::max()
             : std::numeric_limits<SolverScore>::min();
  }

  void set(NodeId node, SolverMinute distance_value, SolverScore priority_value,
           EdgeId predecessor_value,
           SolverMinute departure_value = 0U) {
    generations[node] = generation;
    distances[node] = distance_value;
    priorities[node] = priority_value;
    predecessors[node] = predecessor_value;
    departures[node] = departure_value;
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

struct TraversalStep {
  SolverMinute next{};
  SolverMinute departure{};
  SolverScore penalty{};
};

[[nodiscard]] auto capacity_enabled(const CapacitySearch* capacity) -> bool {
  return capacity != nullptr && capacity->store != nullptr &&
         capacity->store->enabled();
}

[[nodiscard]] auto capacity_allowed(const CapacitySearch* capacity,
                                    const EdgeId edge,
                                    const SolverMinute departure) -> bool {
  if (!capacity_enabled(capacity)) {
    return true;
  }
  return capacity->store->available(
      CapacityInstanceKey{.edge = edge, .departure = departure},
      capacity->units,
      capacity->allow_protected_capacity);
}

[[nodiscard]] auto edge_excluded(const CapacitySearch* capacity,
                                 const EdgeId edge) -> bool {
  if (capacity == nullptr || capacity->excluded_edges.empty()) {
    return false;
  }
  return std::ranges::find(capacity->excluded_edges, edge) !=
         capacity->excluded_edges.end();
}

[[nodiscard]] auto capacity_penalty(const CapacitySearch* capacity,
                                    const EdgeId edge,
                                    const SolverMinute departure)
    -> SolverScore {
  if (!capacity_enabled(capacity)) {
    return 0;
  }
  return capacity->store->penalty_minutes(
      CapacityInstanceKey{.edge = edge, .departure = departure},
      capacity->units,
      capacity->allow_protected_capacity,
      capacity->price_weight,
      capacity->protection_weight);
}

[[nodiscard]] auto search_weeks(const CapacitySearch* capacity) -> SolverMinute {
  if (!capacity_enabled(capacity)) {
    return 1U;
  }
  return std::clamp<SolverMinute>(capacity->search_weeks, 1U, 52U);
}

template <PathTraversalMode P>
[[nodiscard]] auto traverse(SolverMinute start, const SolverEdgeHot& edge,
                            const CapacitySearch* capacity)
    -> std::optional<TraversalStep> {
  if (edge_excluded(capacity, edge.id)) {
    return std::nullopt;
  }
  const auto start_week = minute_of_week(start);
  if constexpr (P == PathTraversalMode::FORWARD) {
    if (edge.forward_schedule_count == 0) {
      return std::nullopt;
    }
    const auto begin = edge.forward_schedule.begin();
    const auto end = begin + edge.forward_schedule_count;
    const auto weeks = search_weeks(capacity);
    for (SolverMinute week = 0; week <= weeks; ++week) {
      for (auto schedule = begin; schedule != end; ++schedule) {
        const auto raw_wait =
            static_cast<std::int64_t>(*schedule) -
            static_cast<std::int64_t>(start_week) +
            (static_cast<std::int64_t>(week) * MINUTES_PER_WEEK);
        if (raw_wait < 0) {
          continue;
        }
        const auto wait = static_cast<SolverMinute>(raw_wait);
        const auto processing_start = start + wait;
        const auto departure =
            processing_start + edge.reverse_outbound_latency;
        if (!capacity_allowed(capacity, edge.id, departure)) {
          continue;
        }
        return TraversalStep{
            .next = processing_start + edge.forward_duration,
            .departure = departure,
            .penalty = capacity_penalty(capacity, edge.id, departure),
        };
      }
    }
    return std::nullopt;
  } else {
    if (edge.reverse_schedule_count == 0) {
      return std::nullopt;
    }
    const auto begin = edge.reverse_schedule.begin();
    const auto end = begin + edge.reverse_schedule_count;
    const auto weeks = search_weeks(capacity);
    for (SolverMinute week = 0; week <= weeks; ++week) {
      for (auto schedule = end; schedule != begin;) {
        --schedule;
        const auto raw_wait =
            static_cast<std::int64_t>(start_week) -
            static_cast<std::int64_t>(*schedule) +
            (static_cast<std::int64_t>(week) * MINUTES_PER_WEEK);
        if (raw_wait < 0) {
          continue;
        }
        const auto wait = static_cast<SolverMinute>(raw_wait);
        const auto next = static_cast<std::int64_t>(start) -
                          static_cast<std::int64_t>(wait) -
                          static_cast<std::int64_t>(edge.reverse_duration);
        const auto source_time =
            static_cast<SolverMinute>(std::max<std::int64_t>(0, next));
        const auto departure = source_time + edge.reverse_outbound_latency;
        if (!capacity_allowed(capacity, edge.id, departure)) {
          continue;
        }
        return TraversalStep{
            .next = source_time,
            .departure = departure,
            .penalty = capacity_penalty(capacity, edge.id, departure),
        };
      }
    }
    return std::nullopt;
  }
}

template <PathTraversalMode P>
struct HeapCompare {
  auto operator()(const HeapEntry& lhs, const HeapEntry& rhs) const -> bool {
    if constexpr (P == PathTraversalMode::FORWARD) {
      return lhs.priority > rhs.priority;
    } else {
      return lhs.priority < rhs.priority;
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
    scratch.forward_buckets[entry.priority].push_back(
        {entry.node, entry.distance});
  } else {
    scratch.reverse_buckets[entry.priority].push_back(
        {entry.node, entry.distance});
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
    const auto [node, distance] = bucket->second.back();
    const auto priority = bucket->first;
    bucket->second.pop_back();
    if (bucket->second.empty()) {
      scratch.forward_buckets.erase(bucket);
    }
    return {.priority = priority, .distance = distance, .node = node};
  } else {
    auto bucket = scratch.reverse_buckets.begin();
    const auto [node, distance] = bucket->second.back();
    const auto priority = bucket->first;
    bucket->second.pop_back();
    if (bucket->second.empty()) {
      scratch.reverse_buckets.erase(bucket);
    }
    return {.priority = priority, .distance = distance, .node = node};
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

[[nodiscard]] auto aggregate_instances(
    std::span<const CapacityInstanceKey> instances,
    CapacityUnits units)
    -> std::unordered_map<CapacityInstanceKey, CapacityUnits, CapacityInstanceKeyHash> {
  std::unordered_map<CapacityInstanceKey, CapacityUnits, CapacityInstanceKeyHash>
      requested;
  requested.reserve(instances.size());
  for (const auto& instance : instances) {
    requested[instance] += units;
  }
  return requested;
}

[[nodiscard]] auto reservation_requirements(const CapacityReservation& reservation)
    -> std::unordered_map<CapacityInstanceKey, CapacityUnits, CapacityInstanceKeyHash> {
  return aggregate_instances(std::span<const CapacityInstanceKey>{
                               reservation.instances},
                             reservation.units);
}

	[[nodiscard]] auto unprotected_remaining(const CapacityBucket& bucket)
	    -> CapacityUnits {
  const auto used = bucket.reserved + bucket.consumed;
  if (used >= bucket.capacity) {
    return 0;
  }
  const auto remaining = bucket.capacity - used;
	  return remaining <= bucket.protected_units ? 0
	                                             : remaining - bucket.protected_units;
	}

auto normalize_bucket(CapacityBucket bucket,
                      CapacityStoreMetrics* metrics = nullptr) -> CapacityBucket {
  bool clamped = false;
  if (bucket.reserved > bucket.capacity) {
    bucket.reserved = bucket.capacity;
    clamped = true;
  }
  if (bucket.consumed > bucket.capacity) {
    bucket.consumed = bucket.capacity;
    clamped = true;
  }
  if (bucket.reserved + bucket.consumed > bucket.capacity) {
    bucket.reserved = bucket.capacity - bucket.consumed;
    clamped = true;
  }
  if (bucket.protected_units > bucket.capacity) {
    bucket.protected_units = bucket.capacity;
    clamped = true;
  }
  bucket.price = std::max(0.0, bucket.price);
  if (clamped && metrics != nullptr) {
    ++metrics->invalid_buckets_clamped;
  }
  return bucket;
}

} // namespace

void CapacityReservationStore::set_enabled(const bool enabled) {
  std::scoped_lock lock(m_mutex);
  m_enabled = enabled;
}

auto CapacityReservationStore::enabled() const -> bool {
  std::scoped_lock lock(m_mutex);
  return m_enabled;
}

void CapacityReservationStore::set_missing_capacity_allowed(const bool allowed) {
  std::scoped_lock lock(m_mutex);
  m_missing_capacity_allowed = allowed;
}

auto CapacityReservationStore::missing_capacity_allowed() const -> bool {
  std::scoped_lock lock(m_mutex);
  return m_missing_capacity_allowed;
}

void CapacityReservationStore::set_default_capacity(
    std::optional<CapacityUnits> capacity) {
  std::scoped_lock lock(m_mutex);
  m_default_capacity = capacity;
}

auto CapacityReservationStore::default_capacity() const
    -> std::optional<CapacityUnits> {
  std::scoped_lock lock(m_mutex);
  return m_default_capacity;
}

void CapacityReservationStore::set_capacity(const CapacityInstanceKey key,
                                            const CapacityUnits capacity) {
  std::scoped_lock lock(m_mutex);
  auto& bucket = m_buckets[key];
  bucket.capacity = capacity;
  bucket = normalize_bucket(bucket, &m_metrics);
}

void CapacityReservationStore::set_bucket(const CapacityInstanceKey key,
                                          CapacityBucket bucket) {
  std::scoped_lock lock(m_mutex);
  m_buckets[key] = normalize_bucket(bucket, &m_metrics);
}

auto CapacityReservationStore::bucket(const CapacityInstanceKey key) const
    -> std::optional<CapacityBucket> {
  std::scoped_lock lock(m_mutex);
  const auto found = m_buckets.find(key);
  if (found == m_buckets.end()) {
    return std::nullopt;
  }
  return found->second;
}

void CapacityReservationStore::set_protected_capacity(
    const CapacityInstanceKey key,
    const CapacityUnits units) {
  std::scoped_lock lock(m_mutex);
  auto& bucket = m_buckets[key];
  bucket.protected_units = std::min(units, bucket.capacity);
}

auto CapacityReservationStore::protected_capacity(
    const CapacityInstanceKey key) const -> CapacityUnits {
  std::scoped_lock lock(m_mutex);
  if (const auto found = m_buckets.find(key); found != m_buckets.end()) {
    return found->second.protected_units;
  }
  return 0;
}

void CapacityReservationStore::set_price(const CapacityInstanceKey key,
                                         const double price) {
  std::scoped_lock lock(m_mutex);
  m_buckets[key].price = std::max(0.0, price);
}

auto CapacityReservationStore::price(const CapacityInstanceKey key) const
    -> double {
  std::scoped_lock lock(m_mutex);
  if (const auto found = m_buckets.find(key); found != m_buckets.end()) {
    return found->second.price;
  }
  return 0.0;
}

auto CapacityReservationStore::remaining(const CapacityInstanceKey key) const
    -> std::optional<CapacityUnits> {
  std::scoped_lock lock(m_mutex);
  const auto found = m_buckets.find(key);
  if (found == m_buckets.end()) {
    return m_default_capacity;
  }
  const auto& bucket = found->second;
  const auto used = bucket.reserved + bucket.consumed;
  return used >= bucket.capacity ? CapacityUnits{0} : bucket.capacity - used;
}

auto CapacityReservationStore::available(const CapacityInstanceKey key,
                                         const CapacityUnits units,
                                         const bool allow_protected_capacity) const
    -> bool {
  if (units == 0) {
    return true;
  }

  std::scoped_lock lock(m_mutex);
  const auto found = m_buckets.find(key);
  if (found == m_buckets.end()) {
    if (m_default_capacity.has_value()) {
      return *m_default_capacity >= units;
    }
    return m_missing_capacity_allowed;
  }

  const auto& bucket = found->second;
  const auto total_remaining =
      bucket.capacity >= bucket.reserved + bucket.consumed
          ? bucket.capacity - bucket.reserved - bucket.consumed
          : CapacityUnits{0};
  const auto unprotected = unprotected_remaining(bucket);
  if (allow_protected_capacity) {
    if (total_remaining >= units && unprotected < units) {
      ++m_metrics.protected_capacity_used;
    }
    return total_remaining >= units;
  }
  if (unprotected < units && total_remaining >= units) {
    ++m_metrics.protected_capacity_saved;
  }
  return unprotected >= units;
}

auto CapacityReservationStore::penalty_minutes(
    const CapacityInstanceKey key,
    const CapacityUnits units,
    const bool allow_protected_capacity,
    const double price_weight,
    const double protection_weight) const -> SolverScore {
  if (price_weight <= 0.0 && protection_weight <= 0.0) {
    return 0;
  }
  std::scoped_lock lock(m_mutex);
  const auto found = m_buckets.find(key);
  if (found == m_buckets.end()) {
    return 0;
  }
  const auto& bucket = found->second;
  double penalty = bucket.price * price_weight;
  if (!allow_protected_capacity && protection_weight > 0.0) {
    const auto available_without_protection = unprotected_remaining(bucket);
    if (available_without_protection < units) {
      penalty += protection_weight *
                 static_cast<double>(units - available_without_protection);
    }
  }
  if (penalty <= 0.0) {
    return 0;
  }
  return static_cast<SolverScore>(std::llround(penalty));
}

auto CapacityReservationStore::reserve(
    std::span<const CapacityInstanceKey> instances,
    const CapacityUnits units) -> std::optional<CapacityReservation> {
  if (instances.empty() || units == 0) {
    return CapacityReservation{};
  }

  const auto requested = aggregate_instances(instances, units);

  std::scoped_lock lock(m_mutex);
  for (const auto& [instance, requested_units] : requested) {
    const auto found = m_buckets.find(instance);
    if (found == m_buckets.end()) {
      if (!m_default_capacity.has_value()) {
        if (m_missing_capacity_allowed) {
          continue;
        }
        return std::nullopt;
      }
      if (*m_default_capacity < requested_units) {
        return std::nullopt;
      }
      continue;
    }

    const auto& bucket = found->second;
    if (unprotected_remaining(bucket) < requested_units) {
      return std::nullopt;
    }
  }

  for (const auto& [instance, requested_units] : requested) {
    auto found = m_buckets.find(instance);
    if (found == m_buckets.end()) {
      if (!m_default_capacity.has_value()) {
        continue;
      }
      found = m_buckets
                .emplace(instance,
                         CapacityBucket{
                             .capacity = *m_default_capacity,
                             .reserved = 0,
                             .consumed = 0,
                             .protected_units = 0,
                             .price = 0.0,
                         })
                .first;
    }
    found->second.reserved += requested_units;
  }

  CapacityReservation reservation;
  reservation.instances.assign(instances.begin(), instances.end());
  reservation.units = units;
  const auto now_minutes = std::chrono::floor<std::chrono::minutes>(
      std::chrono::system_clock::now().time_since_epoch());
  const auto clamped_minutes = std::min<std::uint64_t>(
      static_cast<std::uint64_t>(now_minutes.count()),
      std::numeric_limits<SolverMinute>::max());
  reservation.reserved_at =
      CLOCK{CLOCK::duration{static_cast<SolverMinute>(clamped_minutes)}};
  return reservation;
}

auto CapacityReservationStore::reserve_for(
	    std::string owner,
	    std::span<const CapacityInstanceKey> instances,
	    const CapacityUnits units,
	    CapacityReservationOptions options) -> std::optional<CapacityReservation> {
	  if (owner.empty()) {
	    return reserve(instances, units);
	  }

  if (instances.empty() || units == 0) {
    return CapacityReservation{};
  }

  const auto requested = aggregate_instances(instances, units);
  std::scoped_lock lock(m_mutex);

	  auto previous = m_reservations.find(owner);
	  std::optional<CapacityReservation> previous_reservation;
	  if (previous != m_reservations.end()) {
	    previous_reservation = previous->second;
	    if (options.reserved_at >= previous->second.frozen_at) {
	      const auto previous_requirements =
	        reservation_requirements(previous->second);
	      if (previous->second.units != units || previous_requirements != requested) {
	        ++m_metrics.reservation_failures;
	        return std::nullopt;
	      }
	      return previous->second;
	    }
	    for (const auto& [instance, previous_units] :
	         reservation_requirements(previous->second)) {
      if (auto bucket = m_buckets.find(instance); bucket != m_buckets.end()) {
        bucket->second.reserved =
            bucket->second.reserved > previous_units
                ? bucket->second.reserved - previous_units
                : CapacityUnits{0};
      }
    }
  }

  const auto restore_previous = [&]() {
    if (!previous_reservation.has_value()) {
      return;
    }
    for (const auto& [instance, previous_units] :
         reservation_requirements(*previous_reservation)) {
      m_buckets[instance].reserved += previous_units;
    }
  };

  for (const auto& [instance, requested_units] : requested) {
    const auto found = m_buckets.find(instance);
    if (found == m_buckets.end()) {
      if (!m_default_capacity.has_value()) {
	        if (m_missing_capacity_allowed) {
	          continue;
	        }
	        restore_previous();
	        ++m_metrics.reservation_failures;
	        return std::nullopt;
	      }
	      if (*m_default_capacity < requested_units) {
	        restore_previous();
	        ++m_metrics.reservation_failures;
	        return std::nullopt;
	      }
      continue;
    }
	    if (unprotected_remaining(found->second) < requested_units) {
	      restore_previous();
	      ++m_metrics.reservation_failures;
	      return std::nullopt;
	    }
  }

  for (const auto& [instance, requested_units] : requested) {
    auto found = m_buckets.find(instance);
    if (found == m_buckets.end()) {
      if (!m_default_capacity.has_value()) {
        continue;
      }
      found = m_buckets
                .emplace(instance,
                         CapacityBucket{
                             .capacity = *m_default_capacity,
                             .reserved = 0,
                             .consumed = 0,
                             .protected_units = 0,
                             .price = 0.0,
                         })
                .first;
    }
    found->second.reserved += requested_units;
  }

	  CapacityReservation reservation;
	  reservation.instances.assign(instances.begin(), instances.end());
	  reservation.units = units;
	  reservation.reserved_at = options.reserved_at;
	  reservation.expires_at = options.expires_at;
	  reservation.frozen_at = options.frozen_at;
	  reservation.payload = std::move(options.payload);
	  m_reservations[std::move(owner)] = reservation;
	  if (previous_reservation.has_value()) {
	    ++m_metrics.reservations_replaced;
	  } else {
	    ++m_metrics.reservations_created;
	  }
	  return reservation;
	}

void CapacityReservationStore::release(
    const CapacityReservation& reservation) {
  if (reservation.instances.empty() || reservation.units == 0) {
    return;
  }

  std::scoped_lock lock(m_mutex);
  for (const auto& instance : reservation.instances) {
    const auto found = m_buckets.find(instance);
    if (found == m_buckets.end()) {
      continue;
    }
    found->second.reserved =
        found->second.reserved > reservation.units
            ? found->second.reserved - reservation.units
            : CapacityUnits{0};
  }
}

auto CapacityReservationStore::release(std::string_view owner)
    -> std::optional<CapacityReservation> {
  std::scoped_lock lock(m_mutex);
  const auto found = m_reservations.find(std::string{owner});
  if (found == m_reservations.end()) {
    return std::nullopt;
  }
  const auto reservation_copy = found->second;
  for (const auto& [instance, units] : reservation_requirements(reservation_copy)) {
    const auto bucket = m_buckets.find(instance);
    if (bucket == m_buckets.end()) {
      continue;
    }
    bucket->second.reserved = bucket->second.reserved > units
                                ? bucket->second.reserved - units
                                : CapacityUnits{0};
  }
	  m_reservations.erase(found);
	  ++m_metrics.reservations_released;
	  return reservation_copy;
	}

auto CapacityReservationStore::consume(std::string_view owner)
    -> std::optional<CapacityReservation> {
  std::scoped_lock lock(m_mutex);
  const auto found = m_reservations.find(std::string{owner});
  if (found == m_reservations.end()) {
    return std::nullopt;
  }
  const auto reservation_copy = found->second;
  for (const auto& [instance, units] : reservation_requirements(reservation_copy)) {
    auto& bucket = m_buckets[instance];
    bucket.reserved = bucket.reserved > units ? bucket.reserved - units
                                              : CapacityUnits{0};
	    bucket.consumed += units;
	    bucket = normalize_bucket(bucket, &m_metrics);
	  }
	  m_reservations.erase(found);
	  ++m_metrics.reservations_consumed;
	  return reservation_copy;
	}

auto CapacityReservationStore::expire_before(CLOCK deadline)
    -> std::vector<std::pair<std::string, CapacityReservation>> {
  std::scoped_lock lock(m_mutex);
  std::vector<std::pair<std::string, CapacityReservation>> expired;
  for (auto iterator = m_reservations.begin(); iterator != m_reservations.end();) {
    if (iterator->second.expires_at > deadline) {
      ++iterator;
      continue;
    }
    auto reservation_copy = iterator->second;
    for (const auto& [instance, units] :
         reservation_requirements(reservation_copy)) {
      const auto bucket = m_buckets.find(instance);
      if (bucket == m_buckets.end()) {
        continue;
      }
      bucket->second.reserved = bucket->second.reserved > units
                                  ? bucket->second.reserved - units
                                  : CapacityUnits{0};
    }
    expired.push_back({iterator->first, std::move(reservation_copy)});
    iterator = m_reservations.erase(iterator);
  }
  m_metrics.reservations_expired += expired.size();
  return expired;
}

void CapacityReservationStore::clear() {
  std::scoped_lock lock(m_mutex);
  m_buckets.clear();
  m_reservations.clear();
}

auto CapacityReservationStore::reservation(std::string_view owner) const
    -> std::optional<CapacityReservation> {
  std::scoped_lock lock(m_mutex);
  const auto found = m_reservations.find(std::string{owner});
  if (found == m_reservations.end()) {
    return std::nullopt;
  }
	  return found->second;
	}

auto CapacityReservationStore::metrics() const -> CapacityStoreMetrics {
  std::scoped_lock lock(m_mutex);
  return m_metrics;
}

auto CapacityReservationStore::buckets_snapshot() const
    -> std::vector<std::pair<CapacityInstanceKey, CapacityBucket>> {
  std::scoped_lock lock(m_mutex);
  std::vector<std::pair<CapacityInstanceKey, CapacityBucket>> snapshot;
  snapshot.reserve(m_buckets.size());
  for (const auto& bucket : m_buckets) {
    snapshot.push_back(bucket);
  }
  return snapshot;
}

auto CapacityReservationStore::reservations_snapshot() const
    -> std::vector<std::pair<std::string, CapacityReservation>> {
  std::scoped_lock lock(m_mutex);
  std::vector<std::pair<std::string, CapacityReservation>> snapshot;
  snapshot.reserve(m_reservations.size());
  for (const auto& reservation_entry : m_reservations) {
    snapshot.push_back(reservation_entry);
  }
  return snapshot;
}

auto Solver::valid_node(const NodeId node) const -> bool {
  return node < m_nodes.size();
}

void Solver::invalidate_graph() {
  m_csr_dirty.store(true, std::memory_order_release);
}

void Solver::rebuild_csr() const {
  if (!m_csr_dirty.load(std::memory_order_acquire)) {
    return;
  }

  std::scoped_lock lock(m_csr_mutex);
  if (!m_csr_dirty.load(std::memory_order_relaxed)) {
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

  m_csr_dirty.store(false, std::memory_order_release);
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
  const auto reverse_outbound_latency = route.source_offset();
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
    const std::vector<EdgeId>& predecessors,
    const std::vector<SolverMinute>& departures) const -> Path {
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
    const auto edge_id = index < scratch.path_edges.size()
                             ? scratch.path_edges[index]
                             : INVALID_EDGE;
    const auto* outbound =
        edge_id != INVALID_EDGE ? &m_edge_details[m_edges[edge_id].cold].edge
                                : nullptr;
    path.steps.push_back(PathStep{
        .node = &m_nodes[node],
        .outbound = outbound,
        .outbound_edge = edge_id,
        .distance = minute_to_clock(distances[node]),
        .departure =
            edge_id == INVALID_EDGE
                ? CLOCK{}
                : minute_to_clock(
                      departures[scratch.path_nodes[index + 1U]]),
    });
  }
  return path;
}

auto Solver::build_reverse_path(
    const NodeId source, const NodeId target,
    const std::vector<SolverMinute>& distances,
    const std::vector<EdgeId>& predecessors,
    const std::vector<SolverMinute>& departures) const -> Path {
  Path path;
  path.steps.reserve(scratch.path_nodes.capacity());

  for (NodeId current = target; current != source;) {
    const EdgeId predecessor = predecessors[current];
    if (predecessor == INVALID_EDGE) {
      return {};
    }

    const auto& edge = m_edges[predecessor];
    const auto departure = departures[current];

    path.steps.push_back(PathStep{
        .node = &m_nodes[current],
        .outbound = &m_edge_details[edge.cold].edge,
        .outbound_edge = predecessor,
        .distance = minute_to_clock(departure),
        .departure = minute_to_clock(departure),
    });
    current = edge.target;
  }

  path.steps.push_back(PathStep{
      .node = &m_nodes[source],
      .outbound = nullptr,
      .outbound_edge = INVALID_EDGE,
      .distance = minute_to_clock(distances[source]),
      .departure = CLOCK{},
  });
  return path;
}

template <PathTraversalMode P, VehicleType V>
auto Solver::find_path_impl(const NodeId source, const NodeId target,
                            CLOCK start,
                            const CapacitySearch* capacity) const -> Path {
  if (!valid_node(source) || !valid_node(target)) {
    return {};
  }

  if constexpr (P == PathTraversalMode::FORWARD) {
    scratch.begin(m_nodes.size(), std::numeric_limits<SolverMinute>::max());
  } else {
    scratch.begin(m_nodes.size(), 0U);
  }

  const auto start_minute = clock_to_minute(start);
  const auto start_priority = static_cast<SolverScore>(start_minute);
  scratch.set(source, start_minute, start_priority, INVALID_EDGE);
  queue_push<P>({.priority = start_priority,
                 .distance = start_minute,
                 .node = source});

  while (!queue_empty<P>()) {
    const auto current = queue_pop<P>();
    if (current.distance != scratch.distance(current.node) ||
        current.priority != scratch.priority(current.node)) {
      continue;
    }
    if (current.node == target) {
      if constexpr (P == PathTraversalMode::FORWARD) {
        return build_forward_path(source, target, scratch.distances,
                                  scratch.predecessors,
                                  scratch.departures);
      } else {
        return build_reverse_path(source, target, scratch.distances,
                                  scratch.predecessors,
                                  scratch.departures);
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
      const auto traversal = traverse<P>(current.distance, edge, capacity);
      if (!traversal.has_value()) {
        continue;
      }
      const SolverMinute next = traversal->next;
      const auto elapsed = [&]() -> SolverScore {
        if constexpr (P == PathTraversalMode::FORWARD) {
          return static_cast<SolverScore>(next) -
                 static_cast<SolverScore>(current.distance);
        } else {
          return static_cast<SolverScore>(current.distance) -
                 static_cast<SolverScore>(next);
        }
      }();
      const auto next_priority = [&]() -> SolverScore {
        if constexpr (P == PathTraversalMode::FORWARD) {
          return current.priority + elapsed + traversal->penalty;
        } else {
          return current.priority - elapsed - traversal->penalty;
        }
      }();

      if constexpr (P == PathTraversalMode::FORWARD) {
        if (next_priority >= scratch.priority(next_node)) {
          continue;
        }
      } else {
        if (next_priority <= scratch.priority(next_node)) {
          continue;
        }
      }

      scratch.set(next_node, next, next_priority, edge_id, traversal->departure);
      queue_push<P>({.priority = next_priority,
                     .distance = next,
                     .node = next_node});
    }
  }

  return {};
}

template <>
auto Solver::find_path<PathTraversalMode::FORWARD, VehicleType::AIR>(
    const NodeId source, const NodeId target, CLOCK start) const
    -> Path {
  return find_path_impl<PathTraversalMode::FORWARD, VehicleType::AIR>(
      source, target, start, nullptr);
}

template <>
auto Solver::find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
    const NodeId source, const NodeId target, CLOCK start) const
    -> Path {
  return find_path_impl<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
      source, target, start, nullptr);
}

template <>
auto Solver::find_path<PathTraversalMode::REVERSE, VehicleType::AIR>(
    const NodeId source, const NodeId target, CLOCK start) const
    -> Path {
  return find_path_impl<PathTraversalMode::REVERSE, VehicleType::AIR>(
      source, target, start, nullptr);
}

template <>
auto Solver::find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
    const NodeId source, const NodeId target, CLOCK start) const
    -> Path {
  return find_path_impl<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
      source, target, start, nullptr);
}

template <>
auto Solver::find_path<PathTraversalMode::FORWARD, VehicleType::AIR>(
    const NodeId source, const NodeId target, CLOCK start,
    const CapacitySearch& capacity) const -> Path {
  return find_path_impl<PathTraversalMode::FORWARD, VehicleType::AIR>(
      source, target, start, &capacity);
}

template <>
auto Solver::find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
    const NodeId source, const NodeId target, CLOCK start,
    const CapacitySearch& capacity) const -> Path {
  return find_path_impl<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
      source, target, start, &capacity);
}

template <>
auto Solver::find_path<PathTraversalMode::REVERSE, VehicleType::AIR>(
    const NodeId source, const NodeId target, CLOCK start,
    const CapacitySearch& capacity) const -> Path {
  return find_path_impl<PathTraversalMode::REVERSE, VehicleType::AIR>(
      source, target, start, &capacity);
}

template <>
auto Solver::find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
    const NodeId source, const NodeId target, CLOCK start,
    const CapacitySearch& capacity) const -> Path {
  return find_path_impl<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
      source, target, start, &capacity);
}

namespace {

[[nodiscard]] auto path_signature(const Path& path) -> std::string {
  std::string signature;
  for (const auto& step : path.steps) {
    if (step.outbound_edge == INVALID_EDGE) {
      continue;
    }
    if (!signature.empty()) {
      signature.push_back(',');
    }
    signature += std::to_string(step.outbound_edge);
    signature.push_back('@');
    signature += std::to_string(step.departure.time_since_epoch().count());
  }
  return signature;
}

[[nodiscard]] auto path_route_signature(const Path& path) -> std::string {
  std::string signature;
  for (const auto& step : path.steps) {
    if (step.outbound_edge == INVALID_EDGE) {
      continue;
    }
    if (!signature.empty()) {
      signature.push_back(',');
    }
    signature += std::to_string(step.outbound_edge);
  }
  return signature;
}

[[nodiscard]] auto path_hub_signature(const Path& path) -> std::string {
  std::string signature;
  for (const auto& step : path.steps) {
    if (step.node == nullptr) {
      continue;
    }
    if (!signature.empty()) {
      signature.push_back(',');
    }
    signature += step.node->code;
  }
  return signature;
}

[[nodiscard]] auto path_departure_day_signature(const Path& path)
    -> std::string {
  std::string signature;
  for (const auto& step : path.steps) {
    if (step.outbound_edge == INVALID_EDGE) {
      continue;
    }
    if (!signature.empty()) {
      signature.push_back(',');
    }
    const auto day =
        (step.departure.time_since_epoch().count() / (24 * 60)) % 7;
    signature += std::format("{}@{}", step.outbound_edge, day);
  }
  return signature;
}

[[nodiscard]] auto path_slack_signature(const Path& path,
                                        const CLOCK deadline) -> std::string {
  if (deadline == CLOCK::max() || path.empty()) {
    return {};
  }
  const auto arrival = path.back().distance;
  const auto slack_minutes =
      deadline >= arrival ? (deadline - arrival).count() : 0;
  constexpr SolverMinute SLACK_BUCKET_MINUTES = 60;
  return std::to_string(slack_minutes / SLACK_BUCKET_MINUTES);
}

} // namespace

template <PathTraversalMode P, VehicleType V>
auto Solver::find_path_candidates(
    const NodeId source,
    const NodeId target,
    CLOCK start,
    const CapacitySearch& capacity,
    const PathCandidateOptions& options) const -> std::vector<Path> {
  const auto max_candidates =
      std::clamp<std::size_t>(options.max_candidates, 1U, 64U);
  const auto max_expansions =
      std::max<std::size_t>(1U, options.max_expansions);
  auto started_at = std::chrono::steady_clock::now();
  PathCandidateMetrics metrics{
      .requested = max_candidates,
  };
  CapacitySearch base_search = capacity;
  base_search.search_weeks = options.search_weeks == 0
                               ? capacity.search_weeks
                               : options.search_weeks;
  base_search.allow_protected_capacity = options.allow_protected_capacity;
  base_search.price_weight = options.price_weight;
  base_search.protection_weight = options.protection_weight;

  std::vector<Path> candidates;
  candidates.reserve(max_candidates);
  std::unordered_set<std::string> seen;
  std::unordered_set<std::string> seen_routes;
  std::unordered_set<std::string> seen_hubs;
  std::unordered_set<std::string> seen_departure_days;
  std::unordered_set<std::string> seen_slacks;
  std::unordered_set<EdgeId> attempted_exclusions;

  const auto finish = [&]() -> std::vector<Path> {
    metrics.generated = candidates.size();
    metrics.latency_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started_at)
            .count();
    if (options.metrics != nullptr) {
      *options.metrics = metrics;
    }
    return candidates;
  };

  auto add_candidate = [&](Path path) -> bool {
    if (path.empty()) {
      return false;
    }
    const auto signature = path_signature(path);
    if (signature.empty() || seen.contains(signature)) {
      ++metrics.duplicates;
      return false;
    }
    const auto route_signature = path_route_signature(path);
    const auto hub_signature = path_hub_signature(path);
    const auto departure_day_signature = path_departure_day_signature(path);
    const auto slack_signature = path_slack_signature(path, options.deadline);
    const auto route_duplicate = seen_routes.contains(route_signature);
    const auto hub_duplicate = seen_hubs.contains(hub_signature);
    const auto departure_day_duplicate =
        seen_departure_days.contains(departure_day_signature);
    const auto slack_duplicate =
        !slack_signature.empty() && seen_slacks.contains(slack_signature);
    metrics.route_duplicates += route_duplicate ? 1U : 0U;
    metrics.hub_duplicates += hub_duplicate ? 1U : 0U;
    metrics.departure_day_duplicates += departure_day_duplicate ? 1U : 0U;
    metrics.slack_duplicates += slack_duplicate ? 1U : 0U;

    bool failed_diversity = true;
    bool checked_diversity = false;
    if (options.diversify_routes) {
      checked_diversity = true;
      failed_diversity = failed_diversity && route_duplicate;
    }
    if (options.diversify_hubs) {
      checked_diversity = true;
      failed_diversity = failed_diversity && hub_duplicate;
    }
    if (options.diversify_departure_days) {
      checked_diversity = true;
      failed_diversity = failed_diversity && departure_day_duplicate;
    }
    if (options.diversify_slack && !slack_signature.empty()) {
      checked_diversity = true;
      failed_diversity = failed_diversity && slack_duplicate;
    }
    if (checked_diversity && failed_diversity) {
      ++metrics.duplicates;
      return false;
    }

    seen.insert(signature);
    seen_routes.insert(route_signature);
    seen_hubs.insert(hub_signature);
    seen_departure_days.insert(departure_day_signature);
    if (!slack_signature.empty()) {
      seen_slacks.insert(slack_signature);
    }
    candidates.push_back(std::move(path));
    return candidates.size() >= max_candidates;
  };

  if (add_candidate(find_path<P, V>(source, target, start, base_search))) {
    return finish();
  }

  for (std::size_t candidate_index = 0;
       candidate_index < candidates.size() && candidates.size() < max_candidates;
       ++candidate_index) {
    const auto candidate = candidates[candidate_index];
    for (const auto& step : candidate.steps) {
      if (step.outbound_edge == INVALID_EDGE ||
          attempted_exclusions.contains(step.outbound_edge)) {
        continue;
      }
      attempted_exclusions.insert(step.outbound_edge);
      if (metrics.expansions >= max_expansions) {
        metrics.expansion_cap_reached = true;
        return finish();
      }
      ++metrics.expansions;
      const std::array<EdgeId, 1> excluded{step.outbound_edge};
      CapacitySearch alternative_search = base_search;
      alternative_search.excluded_edges = std::span<const EdgeId>{excluded};
      if (add_candidate(find_path<P, V>(
              source, target, start, alternative_search))) {
        return finish();
      }
    }
  }

  return finish();
}

template auto
Solver::find_path_candidates<PathTraversalMode::FORWARD, VehicleType::AIR>(
    NodeId source,
    NodeId target,
    CLOCK start,
    const CapacitySearch& capacity,
    const PathCandidateOptions& options) const -> std::vector<Path>;

template auto
Solver::find_path_candidates<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
    NodeId source,
    NodeId target,
    CLOCK start,
    const CapacitySearch& capacity,
    const PathCandidateOptions& options) const -> std::vector<Path>;

template auto
Solver::find_path_candidates<PathTraversalMode::REVERSE, VehicleType::AIR>(
    NodeId source,
    NodeId target,
    CLOCK start,
    const CapacitySearch& capacity,
    const PathCandidateOptions& options) const -> std::vector<Path>;

template auto
Solver::find_path_candidates<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
    NodeId source,
    NodeId target,
    CLOCK start,
    const CapacitySearch& capacity,
    const PathCandidateOptions& options) const -> std::vector<Path>;

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
