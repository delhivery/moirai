module;

#ifdef MOIRAI_WITH_HIGHS
#include <highs/interfaces/highs_c_api.h>
#endif
#ifdef MOIRAI_WITH_ROCKSDB
#include <rocksdb/db.h>
#include <rocksdb/iterator.h>
#include <rocksdb/options.h>
#include <rocksdb/write_batch.h>
#endif

module moirai.capacity_optimizer;

import std;
import moirai.date_utils;
import moirai.json_utils;
import moirai.solver;

namespace moirai {
namespace {

constexpr double INF = 1.0e30;

struct ObjectiveWeights {
  double sla_breach_penalty{};
  double hop_penalty{};
  double lateness_penalty_per_minute{};
};

[[nodiscard]] auto find_units(const Json& object, const char* key)
    -> std::optional<CapacityUnits> {
  const auto parsed = find_integer_member<std::int64_t>(object, key);
  if (!parsed.has_value() || *parsed < 0 ||
      *parsed > std::numeric_limits<CapacityUnits>::max()) {
    return std::nullopt;
  }
  return static_cast<CapacityUnits>(*parsed);
}

[[nodiscard]] auto find_double(const Json& object, const char* key)
    -> std::optional<double> {
  const auto* value = find_member(object, key);
  if (value == nullptr) {
    return std::nullopt;
  }
  if (value->is_double()) {
    return value->get_double().value_unsafe();
  }
  if (const auto integer = get_integer<std::int64_t>(*value);
      integer.has_value()) {
    return static_cast<double>(*integer);
  }
  if (const auto string_value = get_string(*value); string_value.has_value()) {
    double parsed{};
    const auto* begin = string_value->data();
    const auto* end = begin + string_value->size();
    const auto [ptr, error] = std::from_chars(begin, end, parsed);
    if (error == std::errc{} && ptr == end) {
      return parsed;
    }
  }
  return std::nullopt;
}

[[nodiscard]] auto clock_from_minutes(std::uint64_t minutes) -> CLOCK {
  const auto clamped = std::min<std::uint64_t>(
      minutes,
      std::numeric_limits<SolverMinute>::max());
  return CLOCK{CLOCK::duration{static_cast<SolverMinute>(clamped)}};
}

[[nodiscard]] auto clock_minutes(CLOCK timestamp) -> std::uint64_t {
  return static_cast<std::uint64_t>(timestamp.time_since_epoch().count());
}

[[nodiscard]] auto add_units(CapacityUnits lhs, CapacityUnits rhs)
    -> CapacityUnits {
  const auto max = std::numeric_limits<CapacityUnits>::max();
  return max - lhs < rhs ? max : lhs + rhs;
}

[[nodiscard]] auto parse_update_mode(const Json& object) -> CapacityUpdateMode {
  const auto mode = find_string_member(object, "mode")
                      .or_else([&]() {
                        return find_string_member(object, "update_mode");
                      })
                      .or_else([&]() {
                        return find_string_member(object, "op");
                      });
  if (!mode.has_value()) {
    return CapacityUpdateMode::SNAPSHOT;
  }
  std::string normalized{*mode};
  std::ranges::transform(normalized, normalized.begin(), [](unsigned char chr) {
    return static_cast<char>(std::tolower(chr));
  });
  if (normalized == "delta" || normalized == "increment" ||
      normalized == "incremental") {
    return CapacityUpdateMode::DELTA;
  }
  return CapacityUpdateMode::SNAPSHOT;
}

[[nodiscard]] auto forecast_protection_units(
    const CapacityUnits capacity,
    const CapacityProtectionPolicy& policy) -> CapacityUnits {
  if (capacity == 0) {
    return 0;
  }
  const auto ratio = std::clamp(policy.reserve_ratio, 0.0, 1.0);
  CapacityUnits protected_units = 0;
  if (ratio > 0.0) {
    protected_units = static_cast<CapacityUnits>(
        std::ceil(static_cast<double>(capacity) * ratio));
  }
  if (policy.min_protected_units > 0) {
    protected_units =
        std::max(protected_units, policy.min_protected_units);
  }
  if (policy.max_protected_units.has_value()) {
    protected_units = std::min(protected_units, *policy.max_protected_units);
  }
  return std::min(protected_units, capacity);
}

[[nodiscard]] auto parse_key(std::string_view input)
    -> std::optional<CapacityInstanceKey> {
  const auto separator = input.find(':');
  if (separator == std::string_view::npos) {
    return std::nullopt;
  }
  const auto edge = parse_integer<EdgeId>(input.substr(0, separator));
  const auto departure =
      parse_integer<SolverMinute>(input.substr(separator + 1U));
  if (!edge.has_value() || !departure.has_value()) {
    return std::nullopt;
  }
  return CapacityInstanceKey{.edge = *edge, .departure = *departure};
}

[[nodiscard]] auto encode_key(const CapacityInstanceKey key) -> std::string {
  return std::format("{}:{}", key.edge, key.departure);
}

[[nodiscard]] auto encode_bucket_value(const CapacityBucket& bucket)
    -> std::string {
  return std::format("v2 {} {} {} {} {}",
                     bucket.capacity,
                     bucket.reserved,
                     bucket.consumed,
                     bucket.protected_units,
                     bucket.price);
}

[[nodiscard]] auto parse_bucket_value(std::string_view value)
    -> std::optional<CapacityBucket> {
  std::istringstream input{std::string{value}};
  CapacityBucket bucket;
  std::string version_or_capacity;
  input >> version_or_capacity;
  if (!input) {
    return std::nullopt;
  }
  if (version_or_capacity == "v2") {
    input >> bucket.capacity >> bucket.reserved >> bucket.consumed >>
        bucket.protected_units >> bucket.price;
  } else {
    const auto capacity = parse_integer<CapacityUnits>(version_or_capacity);
    if (!capacity.has_value()) {
      return std::nullopt;
    }
    bucket.capacity = *capacity;
    input >> bucket.consumed >> bucket.protected_units >> bucket.price;
    bucket.reserved = 0;
  }
  if (!input) {
    return std::nullopt;
  }
  return bucket;
}

[[nodiscard]] auto encode_reservation_value(
    const CapacityReservation& reservation) -> std::string {
  std::ostringstream output;
  output << "v2 " << reservation.units << ' ' << reservation.instances.size()
         << ' ' << clock_minutes(reservation.reserved_at) << ' '
         << clock_minutes(reservation.expires_at) << ' '
         << clock_minutes(reservation.frozen_at) << ' '
         << std::quoted(reservation.payload);
  for (const auto& instance : reservation.instances) {
    output << ' ' << encode_key(instance);
  }
  return output.str();
}

[[nodiscard]] auto parse_reservation_value(std::string_view value)
    -> std::optional<CapacityReservation> {
  std::istringstream input{std::string{value}};
  CapacityReservation reservation;
  std::size_t count{};
  std::string version_or_units;
  input >> version_or_units;
  if (!input) {
    return std::nullopt;
  }
  if (version_or_units == "v2") {
    std::uint64_t reserved_at{};
    std::uint64_t expires_at{};
    std::uint64_t frozen_at{};
    input >> reservation.units >> count >> reserved_at >> expires_at >>
        frozen_at >> std::quoted(reservation.payload);
    reservation.reserved_at = clock_from_minutes(reserved_at);
    reservation.expires_at = clock_from_minutes(expires_at);
    reservation.frozen_at = clock_from_minutes(frozen_at);
  } else {
    const auto units = parse_integer<CapacityUnits>(version_or_units);
    if (!units.has_value()) {
      return std::nullopt;
    }
    reservation.units = *units;
    input >> count;
  }
  if (!input || reservation.units == 0) {
    return std::nullopt;
  }
  reservation.instances.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    std::string key_value;
    input >> key_value;
    const auto key = parse_key(key_value);
    if (!key.has_value()) {
      return std::nullopt;
    }
    reservation.instances.push_back(*key);
  }
  return reservation;
}

[[nodiscard]] auto reservation_requirements(
    const CapacityReservation& reservation)
    -> std::unordered_map<CapacityInstanceKey,
                          CapacityUnits,
                          CapacityInstanceKeyHash> {
  std::unordered_map<CapacityInstanceKey, CapacityUnits, CapacityInstanceKeyHash>
      requirements;
  requirements.reserve(reservation.instances.size());
  for (const auto& instance : reservation.instances) {
    requirements[instance] += reservation.units;
  }
  return requirements;
}

[[nodiscard]] auto owner_reserved_units(
    const std::vector<std::pair<std::string, CapacityReservation>>& reservations)
    -> std::unordered_map<CapacityInstanceKey,
                          CapacityUnits,
                          CapacityInstanceKeyHash> {
  std::unordered_map<CapacityInstanceKey, CapacityUnits, CapacityInstanceKeyHash>
      reserved;
  for (const auto& [_, reservation] : reservations) {
    for (const auto& [instance, units] : reservation_requirements(reservation)) {
      reserved[instance] += units;
    }
  }
  return reserved;
}

[[nodiscard]] auto subtract_owner_reservations(
    CapacityBucket bucket,
    const CapacityInstanceKey key,
    const std::unordered_map<CapacityInstanceKey,
                             CapacityUnits,
                             CapacityInstanceKeyHash>& owner_reserved)
    -> CapacityBucket {
  const auto found = owner_reserved.find(key);
  if (found == owner_reserved.end()) {
    return bucket;
  }
  bucket.reserved = bucket.reserved > found->second
                      ? bucket.reserved - found->second
                      : CapacityUnits{0};
  return bucket;
}

[[nodiscard]] auto snapshot_backend_for_path(
    const std::filesystem::path& path,
    const CapacityStateBackend requested) -> CapacityStateBackend {
  if (requested != CapacityStateBackend::AUTO) {
    return requested;
  }
#ifdef MOIRAI_WITH_ROCKSDB
  if (path.extension() == ".log" || path.extension() == ".txt" ||
      path.extension() == ".snapshot") {
    return CapacityStateBackend::SNAPSHOT;
  }
  return CapacityStateBackend::ROCKSDB;
#else
  (void)path;
  return CapacityStateBackend::SNAPSHOT;
#endif
}

#ifdef MOIRAI_WITH_ROCKSDB
constexpr std::string_view ROCKSDB_BUCKET_PREFIX = "bucket:";
constexpr std::string_view ROCKSDB_RESERVATION_PREFIX = "reservation:";

[[nodiscard]] auto starts_with(std::string_view value,
                               std::string_view prefix) -> bool {
  return value.substr(0, prefix.size()) == prefix;
}

[[nodiscard]] auto open_capacity_db(const std::filesystem::path& path)
    -> std::unique_ptr<rocksdb::DB> {
  rocksdb::Options options;
  options.create_if_missing = true;
  rocksdb::DB* raw_db = nullptr;
  const auto status = rocksdb::DB::Open(options, path.string(), &raw_db);
  if (!status.ok()) {
    throw std::runtime_error(
        std::format("Unable to open RocksDB capacity state {}: {}",
                    path.string(),
                    status.ToString()));
  }
  return std::unique_ptr<rocksdb::DB>{raw_db};
}

void restore_rocksdb_state(const std::filesystem::path& path,
                           CapacityReservationStore& store) {
  auto db = open_capacity_db(path);
  rocksdb::ReadOptions read_options;
  std::unique_ptr<rocksdb::Iterator> iterator{
      db->NewIterator(read_options)};
  for (iterator->SeekToFirst(); iterator->Valid(); iterator->Next()) {
    const auto key = iterator->key().ToString();
    const auto value = iterator->value().ToString();
    if (starts_with(key, ROCKSDB_BUCKET_PREFIX)) {
      const auto instance =
          parse_key(std::string_view{key}.substr(ROCKSDB_BUCKET_PREFIX.size()));
      const auto bucket = parse_bucket_value(value);
      if (instance.has_value() && bucket.has_value()) {
        store.set_bucket(*instance, *bucket);
      }
    }
  }
  for (iterator->SeekToFirst(); iterator->Valid(); iterator->Next()) {
    const auto key = iterator->key().ToString();
    const auto value = iterator->value().ToString();
    if (!starts_with(key, ROCKSDB_RESERVATION_PREFIX)) {
      continue;
    }
    const auto owner =
        std::string{std::string_view{key}.substr(ROCKSDB_RESERVATION_PREFIX.size())};
    const auto reservation = parse_reservation_value(value);
    if (!owner.empty() && reservation.has_value()) {
      const CapacityReservationOptions options{
          .reserved_at = reservation->reserved_at,
          .expires_at = reservation->expires_at,
          .frozen_at = reservation->frozen_at,
          .payload = reservation->payload,
      };
      (void)store.reserve_for(
          owner,
          std::span<const CapacityInstanceKey>{reservation->instances},
          reservation->units,
          options);
    }
  }
  if (!iterator->status().ok()) {
    throw std::runtime_error(
        std::format("Unable to read RocksDB capacity state {}: {}",
                    path.string(),
                    iterator->status().ToString()));
  }
}

void persist_rocksdb_state(const std::filesystem::path& path,
                           const CapacityReservationStore& store) {
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path());
  }
  auto db = open_capacity_db(path);
  rocksdb::WriteBatch batch;
  rocksdb::ReadOptions read_options;
  std::unique_ptr<rocksdb::Iterator> iterator{
      db->NewIterator(read_options)};
  for (iterator->SeekToFirst(); iterator->Valid(); iterator->Next()) {
    const auto key = iterator->key().ToString();
    if (starts_with(key, ROCKSDB_BUCKET_PREFIX) ||
        starts_with(key, ROCKSDB_RESERVATION_PREFIX)) {
      batch.Delete(key);
    }
  }
  const auto reservations = store.reservations_snapshot();
  const auto owner_reserved = owner_reserved_units(reservations);
  for (const auto& [key, bucket] : store.buckets_snapshot()) {
    const auto persisted_bucket =
        subtract_owner_reservations(bucket, key, owner_reserved);
    batch.Put(std::format("{}{}",
                          ROCKSDB_BUCKET_PREFIX,
                          encode_key(key)),
              encode_bucket_value(persisted_bucket));
  }
  for (const auto& [owner, reservation] : reservations) {
    batch.Put(std::format("{}{}", ROCKSDB_RESERVATION_PREFIX, owner),
              encode_reservation_value(reservation));
  }
  rocksdb::WriteOptions write_options;
  write_options.sync = true;
  const auto status = db->Write(write_options, &batch);
  if (!status.ok()) {
    throw std::runtime_error(
        std::format("Unable to persist RocksDB capacity state {}: {}",
                    path.string(),
                    status.ToString()));
  }
}

void clear_rocksdb_state(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    return;
  }
  auto db = open_capacity_db(path);
  rocksdb::WriteBatch batch;
  rocksdb::ReadOptions read_options;
  std::unique_ptr<rocksdb::Iterator> iterator{
      db->NewIterator(read_options)};
  for (iterator->SeekToFirst(); iterator->Valid(); iterator->Next()) {
    const auto key = iterator->key().ToString();
    if (starts_with(key, ROCKSDB_BUCKET_PREFIX) ||
        starts_with(key, ROCKSDB_RESERVATION_PREFIX)) {
      batch.Delete(key);
    }
  }
  rocksdb::WriteOptions write_options;
  write_options.sync = true;
  const auto status = db->Write(write_options, &batch);
  if (!status.ok()) {
    throw std::runtime_error(
        std::format("Unable to clear RocksDB capacity state {}: {}",
                    path.string(),
                    status.ToString()));
  }
}
#endif

[[nodiscard]] auto candidate_lateness_minutes(
    const OptimizationPathCandidate& candidate) -> std::int64_t {
  if (candidate.deadline == CLOCK::max() || candidate.arrival <= candidate.deadline) {
    return 0;
  }
  return (candidate.arrival - candidate.deadline).count();
}

[[nodiscard]] auto candidate_objective(
    const OptimizationPathCandidate& candidate,
    const ObjectiveWeights& weights) -> double {
  const auto lateness = candidate_lateness_minutes(candidate);
  return (candidate.sla_feasible ? 0.0 : weights.sla_breach_penalty) +
         (static_cast<double>(lateness) *
          weights.lateness_penalty_per_minute) +
         (static_cast<double>(candidate.hops) * weights.hop_penalty) +
         candidate.cost;
}

[[nodiscard]] auto objective_weights(
    std::span<const OptimizationShipment> shipments,
    const RollingHorizonConfig& config) -> ObjectiveWeights {
  double max_cost = 0.0;
  std::uint64_t max_hops = 0;
  std::uint64_t max_lateness = 0;
  for (const auto& shipment : shipments) {
    for (const auto& candidate : shipment.candidates) {
      max_cost = std::max(max_cost, std::abs(candidate.cost));
      max_hops = std::max<std::uint64_t>(max_hops, candidate.hops);
      max_lateness = std::max<std::uint64_t>(
          max_lateness,
          static_cast<std::uint64_t>(
              std::max<std::int64_t>(0, candidate_lateness_minutes(candidate))));
    }
  }

  const auto hop_penalty = std::max(1.0, config.hop_penalty);
  const auto lateness_penalty =
      std::max(hop_penalty * static_cast<double>(max_hops + 1U),
               config.lateness_penalty_per_minute);
  const auto per_shipment_bound =
      max_cost + (hop_penalty * static_cast<double>(max_hops + 1U)) +
      (lateness_penalty * static_cast<double>(max_lateness + 1U)) + 1.0;
  const auto batch_bound =
      per_shipment_bound *
      static_cast<double>(std::max<std::size_t>(1U, shipments.size()));
  return ObjectiveWeights{
      .sla_breach_penalty = std::max(config.sla_breach_penalty, batch_bound),
      .hop_penalty = hop_penalty,
      .lateness_penalty_per_minute = lateness_penalty,
  };
}

[[nodiscard]] auto candidate_requirements(
    const OptimizationPathCandidate& candidate)
    -> std::unordered_map<CapacityInstanceKey, CapacityUnits, CapacityInstanceKeyHash> {
  std::unordered_map<CapacityInstanceKey, CapacityUnits, CapacityInstanceKeyHash>
      requirements;
  requirements.reserve(candidate.capacity_instances.size());
  for (const auto& instance : candidate.capacity_instances) {
    requirements[instance] += candidate.units;
  }
  return requirements;
}

[[nodiscard]] auto candidate_available(
    const OptimizationPathCandidate& candidate,
    const CapacityReservationStore& capacity,
    const std::unordered_map<CapacityInstanceKey,
                             CapacityUnits,
                             CapacityInstanceKeyHash>& tentative) -> bool {
  for (const auto& [instance, units] : candidate_requirements(candidate)) {
    const auto remaining = capacity.remaining(instance);
    if (!remaining.has_value()) {
      if (!capacity.missing_capacity_allowed()) {
        return false;
      }
      continue;
    }
    const auto used = [&]() -> CapacityUnits {
      if (const auto found = tentative.find(instance); found != tentative.end()) {
        return found->second;
      }
      return 0;
    }();
    if (*remaining < used || *remaining - used < units) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] auto greedy_optimize(
    std::span<const OptimizationShipment> shipments,
    const CapacityReservationStore& capacity,
    const RollingHorizonConfig& config) -> RollingHorizonResult {
  RollingHorizonResult result;
  result.chosen_candidate.resize(shipments.size());
  const auto weights = objective_weights(shipments, config);

  std::vector<std::size_t> order(shipments.size());
  std::iota(order.begin(), order.end(), 0U);
  std::ranges::sort(order, [&](const auto lhs, const auto rhs) {
    const auto& left = shipments[lhs];
    const auto& right = shipments[rhs];
    if (left.priority != right.priority) {
      return left.priority > right.priority;
    }
    return left.deadline < right.deadline;
  });

  std::unordered_map<CapacityInstanceKey, CapacityUnits, CapacityInstanceKeyHash>
      tentative;
  for (const auto shipment_index : order) {
    const auto& shipment = shipments[shipment_index];
    std::optional<std::size_t> best_candidate;
    double best_score = INF;
    for (std::size_t candidate_index = 0;
         candidate_index < shipment.candidates.size();
         ++candidate_index) {
      const auto& candidate = shipment.candidates[candidate_index];
      if (!candidate_available(candidate, capacity, tentative)) {
        continue;
      }
      const auto score = candidate_objective(candidate, weights);
      if (score < best_score) {
        best_score = score;
        best_candidate = candidate_index;
      }
    }
    if (!best_candidate.has_value()) {
      continue;
    }
    result.chosen_candidate[shipment_index] = best_candidate;
    for (const auto& [instance, units] :
         candidate_requirements(shipment.candidates[*best_candidate])) {
      tentative[instance] += units;
    }
  }

  for (const auto& [instance, used] : tentative) {
    const auto remaining = capacity.remaining(instance).value_or(used);
    const auto denominator = static_cast<double>(std::max<CapacityUnits>(
        1U,
        remaining + used));
    const auto utilization = static_cast<double>(used) / denominator;
    result.capacity_prices[instance] =
        utilization * utilization * config.capacity_price_scale;
  }
  return result;
}

#ifdef MOIRAI_WITH_HIGHS
class HighsHandle {
private:
  void* m_handle{Highs_create()};

public:
  HighsHandle() = default;
  HighsHandle(const HighsHandle&) = delete;
  auto operator=(const HighsHandle&) -> HighsHandle& = delete;
  ~HighsHandle() {
    if (m_handle != nullptr) {
      Highs_destroy(m_handle);
    }
  }

  [[nodiscard]] auto get() const -> void* { return m_handle; }
};

[[nodiscard]] auto highs_optimize(
    std::span<const OptimizationShipment> shipments,
    const CapacityReservationStore& capacity,
    const RollingHorizonConfig& config) -> std::optional<RollingHorizonResult> {
  HighsHandle highs;
  if (highs.get() == nullptr) {
    return std::nullopt;
  }
  (void)Highs_setBoolOptionValue(highs.get(), "output_flag", 0);
  (void)Highs_changeObjectiveSense(highs.get(), kHighsObjSenseMinimize);

  struct VariableRef {
    std::size_t shipment{};
    std::size_t candidate{};
  };
  std::vector<VariableRef> variables;
  std::vector<double> objective;
  const auto weights = objective_weights(shipments, config);
  HighsInt row_count = 0;
  for (std::size_t shipment_index = 0; shipment_index < shipments.size();
       ++shipment_index) {
    const auto& shipment = shipments[shipment_index];
    for (std::size_t candidate_index = 0;
         candidate_index < shipment.candidates.size();
         ++candidate_index) {
      variables.push_back({.shipment = shipment_index, .candidate = candidate_index});
      objective.push_back(candidate_objective(shipment.candidates[candidate_index],
                                             weights));
      if (Highs_addCol(highs.get(), objective.back(), 0.0, 1.0, 0, nullptr, nullptr) !=
          kHighsStatusOk) {
        return std::nullopt;
      }
      if (Highs_changeColIntegrality(
              highs.get(),
              static_cast<HighsInt>(variables.size() - 1U),
              kHighsVarTypeInteger) != kHighsStatusOk) {
        return std::nullopt;
      }
    }
  }

  for (std::size_t shipment_index = 0; shipment_index < shipments.size();
       ++shipment_index) {
    std::vector<HighsInt> columns;
    std::vector<double> values;
    for (std::size_t column = 0; column < variables.size(); ++column) {
      if (variables[column].shipment == shipment_index) {
        columns.push_back(static_cast<HighsInt>(column));
        values.push_back(1.0);
      }
    }
    if (columns.empty()) {
      continue;
    }
    if (Highs_addRow(highs.get(),
                     1.0,
                     1.0,
                     static_cast<HighsInt>(columns.size()),
                     columns.data(),
                     values.data()) != kHighsStatusOk) {
      return std::nullopt;
    }
    ++row_count;
  }

  std::unordered_map<CapacityInstanceKey, std::vector<std::pair<HighsInt, double>>,
                     CapacityInstanceKeyHash>
      capacity_rows;
  std::unordered_map<CapacityInstanceKey, HighsInt, CapacityInstanceKeyHash>
      capacity_row_indices;
  for (std::size_t column = 0; column < variables.size(); ++column) {
    const auto& variable = variables[column];
    const auto& candidate =
        shipments[variable.shipment].candidates[variable.candidate];
    for (const auto& [instance, units] : candidate_requirements(candidate)) {
      capacity_rows[instance].push_back(
          {static_cast<HighsInt>(column), static_cast<double>(units)});
    }
  }
  for (const auto& [instance, entries] : capacity_rows) {
    const auto remaining = capacity.remaining(instance);
    if (!remaining.has_value()) {
      continue;
    }
    std::vector<HighsInt> columns;
    std::vector<double> values;
    columns.reserve(entries.size());
    values.reserve(entries.size());
    for (const auto& [column, value] : entries) {
      columns.push_back(column);
      values.push_back(value);
    }
    if (Highs_addRow(highs.get(),
                     -INF,
                     static_cast<double>(*remaining),
                     static_cast<HighsInt>(columns.size()),
                     columns.data(),
                     values.data()) != kHighsStatusOk) {
      return std::nullopt;
    }
    capacity_row_indices[instance] = row_count;
    ++row_count;
  }

  if (Highs_runQuiet(highs.get()) != kHighsStatusOk) {
    return std::nullopt;
  }

  std::vector<double> values(variables.size());
  std::vector<double> row_duals(static_cast<std::size_t>(row_count));
  if (Highs_getSolution(highs.get(),
                        values.data(),
                        nullptr,
                        nullptr,
                        row_duals.data()) != kHighsStatusOk) {
    return std::nullopt;
  }

  RollingHorizonResult result;
  result.used_highs = true;
  result.optimal = true;
  result.chosen_candidate.resize(shipments.size());
  std::unordered_map<CapacityInstanceKey, CapacityUnits, CapacityInstanceKeyHash>
      used_capacity;
  for (std::size_t column = 0; column < variables.size(); ++column) {
    if (values[column] < 0.5) {
      continue;
    }
    const auto& variable = variables[column];
    result.chosen_candidate[variable.shipment] = variable.candidate;
    for (const auto& [instance, units] :
         candidate_requirements(
             shipments[variable.shipment].candidates[variable.candidate])) {
      used_capacity[instance] += units;
    }
  }

  for (std::size_t shipment_index = 0; shipment_index < shipments.size();
       ++shipment_index) {
    if (!shipments[shipment_index].candidates.empty() &&
        !result.chosen_candidate[shipment_index].has_value()) {
      return std::nullopt;
    }
  }

  for (const auto& [instance, used] : used_capacity) {
    const auto remaining = capacity.remaining(instance).value_or(used);
    const auto denominator = static_cast<double>(std::max<CapacityUnits>(
        1U,
        remaining + used));
    const auto utilization = static_cast<double>(used) / denominator;
    result.capacity_prices[instance] =
        utilization * utilization * config.capacity_price_scale;
  }
  for (const auto& [instance, row] : capacity_row_indices) {
    const auto row_index = static_cast<std::size_t>(row);
    if (row_index >= row_duals.size()) {
      continue;
    }
    const auto dual_price = std::abs(row_duals[row_index]) *
                            config.capacity_price_scale;
    if (std::isfinite(dual_price) && dual_price > 0.0) {
      result.capacity_prices[instance] = dual_price;
    }
  }
  return result;
}
#endif

} // namespace

CapacityStateLog::CapacityStateLog(std::filesystem::path path,
                                   const CapacityStateBackend backend)
    : m_path(std::move(path)), m_backend(backend) {}

auto CapacityStateLog::path() const -> const std::filesystem::path& {
  return m_path;
}

auto CapacityStateLog::backend() const -> CapacityStateBackend {
  return snapshot_backend_for_path(m_path, m_backend);
}

void CapacityStateLog::clear() const {
  std::scoped_lock lock(m_mutex);
  if (m_path.empty()) {
    return;
  }
  const auto active_backend = backend();
#ifdef MOIRAI_WITH_ROCKSDB
  if (active_backend == CapacityStateBackend::ROCKSDB) {
    clear_rocksdb_state(m_path);
    return;
  }
#else
  if (active_backend == CapacityStateBackend::ROCKSDB) {
    throw std::runtime_error(
        "RocksDB capacity state requested, but this build lacks RocksDB");
  }
#endif
  std::filesystem::remove(m_path);
}

void CapacityStateLog::restore(CapacityReservationStore& store) const {
  std::scoped_lock lock(m_mutex);
  if (m_path.empty()) {
    return;
  }
  const auto active_backend = backend();
#ifdef MOIRAI_WITH_ROCKSDB
  if (active_backend == CapacityStateBackend::ROCKSDB) {
    restore_rocksdb_state(m_path, store);
    return;
  }
#else
  if (active_backend == CapacityStateBackend::ROCKSDB) {
    throw std::runtime_error(
        "RocksDB capacity state requested, but this build lacks RocksDB");
  }
#endif

  std::ifstream input(m_path);
  if (!input) {
    return;
  }

  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) {
      continue;
    }
    std::istringstream row(line);
    char type{};
    row >> type;
    if (type == 'B') {
      std::string key_value;
      row >> key_value;
      std::string bucket_value;
      std::getline(row >> std::ws, bucket_value);
      const auto key = parse_key(key_value);
      const auto bucket = parse_bucket_value(bucket_value);
      if (key.has_value() && bucket.has_value()) {
        store.set_bucket(*key, *bucket);
      }
    } else if (type == 'R') {
      std::string owner;
      row >> std::quoted(owner);
      std::string reservation_value;
      std::getline(row >> std::ws, reservation_value);
      const auto reservation = parse_reservation_value(reservation_value);
      if (!owner.empty() && reservation.has_value()) {
        const CapacityReservationOptions options{
            .reserved_at = reservation->reserved_at,
            .expires_at = reservation->expires_at,
            .frozen_at = reservation->frozen_at,
            .payload = reservation->payload,
        };
        (void)store.reserve_for(
            owner,
            std::span<const CapacityInstanceKey>{reservation->instances},
            reservation->units,
            options);
      }
    }
  }
}

void CapacityStateLog::persist_snapshot(
    const CapacityReservationStore& store) const {
  std::scoped_lock lock(m_mutex);
  if (m_path.empty()) {
    return;
  }
  const auto active_backend = backend();
#ifdef MOIRAI_WITH_ROCKSDB
  if (active_backend == CapacityStateBackend::ROCKSDB) {
    persist_rocksdb_state(m_path, store);
    return;
  }
#else
  if (active_backend == CapacityStateBackend::ROCKSDB) {
    throw std::runtime_error(
        "RocksDB capacity state requested, but this build lacks RocksDB");
  }
#endif

  if (m_path.has_parent_path()) {
    std::filesystem::create_directories(m_path.parent_path());
  }
  const auto temp_path = m_path.string() + ".tmp";
  {
    std::ofstream output(temp_path, std::ios::trunc);
    const auto reservations = store.reservations_snapshot();
    const auto owner_reserved = owner_reserved_units(reservations);
    for (const auto& [key, bucket] : store.buckets_snapshot()) {
      const auto persisted_bucket =
          subtract_owner_reservations(bucket, key, owner_reserved);
      output << "B " << encode_key(key) << ' '
             << encode_bucket_value(persisted_bucket) << '\n';
    }
    for (const auto& [owner, reservation] : reservations) {
      output << "R " << std::quoted(owner) << ' '
             << encode_reservation_value(reservation) << '\n';
    }
  }
  std::filesystem::rename(temp_path, m_path);
}

auto parse_capacity_ingestion_record(const Json& object)
    -> std::expected<CapacityIngestionRecord, std::string> {
  const auto edge = find_integer_member<EdgeId>(object, "edge");
  const auto departure = find_integer_member<SolverMinute>(object, "departure");
  if (!edge.has_value() || !departure.has_value()) {
    return std::unexpected{"capacity record requires edge and departure"};
  }
	  return CapacityIngestionRecord{
	      .key = CapacityInstanceKey{.edge = *edge, .departure = *departure},
	      .mode = parse_update_mode(object),
	      .capacity = find_units(object, "capacity"),
	      .reserved = find_units(object, "reserved"),
      .consumed = find_units(object, "consumed"),
      .protected_units = find_units(object, "protected")
                               .or_else([&]() {
                                 return find_units(object, "forecast_protected");
                               })
                               .or_else([&]() {
                                 return find_units(object, "forecast_units");
                               })
                               .or_else([&]() {
                                 return find_units(object, "forecast_demand");
                               })
                               .or_else([&]() {
                                 return find_units(object, "historical_demand");
                               })
                               .or_else([&]() {
                                 return find_units(object, "scenario_demand");
                               })
                               .or_else([&]() {
                                 return find_units(object, "critical_forecast");
                               }),
      .price = find_double(object, "price"),
      .reserve_ratio = find_double(object, "reserve_ratio")
                         .or_else([&]() {
                           return find_double(object, "protection_ratio");
                         })
                         .or_else([&]() {
                           return find_double(object, "forecast_reserve_ratio");
                         }),
      .min_protected_units =
        find_units(object, "min_protected")
          .or_else([&]() {
            return find_units(object, "min_protected_units");
          }),
      .max_protected_units =
        find_units(object, "max_protected")
          .or_else([&]() {
            return find_units(object, "max_protected_units");
          }),
  };
}

void apply_capacity_ingestion_record(
    CapacityReservationStore& store,
    const CapacityIngestionRecord& record,
    const CapacityProtectionPolicy protection_policy) {
	  auto effective_policy = protection_policy;
	  if (record.reserve_ratio.has_value()) {
	    effective_policy.reserve_ratio =
	        std::clamp(*record.reserve_ratio, 0.0, 1.0);
	  }
	  if (record.min_protected_units.has_value()) {
	    effective_policy.min_protected_units = *record.min_protected_units;
	  }
	  if (record.max_protected_units.has_value()) {
	    effective_policy.max_protected_units = *record.max_protected_units;
	  }
	  CapacityBucket updated = store.bucket(record.key).value_or(CapacityBucket{});
	  if (record.mode == CapacityUpdateMode::DELTA) {
	    if (record.capacity.has_value()) {
	      updated.capacity = add_units(updated.capacity, *record.capacity);
	    }
	    if (record.reserved.has_value()) {
	      updated.reserved = add_units(updated.reserved, *record.reserved);
	    }
	    if (record.consumed.has_value()) {
	      updated.consumed = add_units(updated.consumed, *record.consumed);
	    }
	    if (record.protected_units.has_value()) {
	      updated.protected_units =
	          add_units(updated.protected_units, *record.protected_units);
	    } else if (record.capacity.has_value()) {
	      updated.protected_units =
	          forecast_protection_units(updated.capacity, effective_policy);
	    }
	    if (record.price.has_value()) {
	      updated.price = std::max(0.0, updated.price + *record.price);
	    }
	  } else {
	    if (record.capacity.has_value()) {
	      updated.capacity = *record.capacity;
	    }
	    if (record.reserved.has_value()) {
	      updated.reserved = *record.reserved;
	    }
	    if (record.consumed.has_value()) {
	      updated.consumed = *record.consumed;
	    }
	    if (record.protected_units.has_value()) {
	      updated.protected_units = *record.protected_units;
	    } else if (record.capacity.has_value()) {
	      updated.protected_units =
	          forecast_protection_units(updated.capacity, effective_policy);
	    }
	    if (record.price.has_value()) {
	      updated.price = *record.price;
	    }
	  }
	  store.set_bucket(record.key, updated);
	}

RollingHorizonOptimizer::RollingHorizonOptimizer(RollingHorizonConfig config)
    : m_config(std::move(config)) {}

auto RollingHorizonOptimizer::optimize(
    std::span<const OptimizationShipment> shipments,
    const CapacityReservationStore& capacity) const -> RollingHorizonResult {
#ifdef MOIRAI_WITH_HIGHS
  if (m_config.use_highs) {
    if (auto result = highs_optimize(shipments, capacity, m_config);
        result.has_value()) {
      return *result;
    }
  }
#endif
  return greedy_optimize(shipments, capacity, m_config);
}

auto RollingHorizonOptimizer::commit(
    std::span<const OptimizationShipment> shipments,
    const RollingHorizonResult& result,
    CapacityReservationStore& capacity) const -> std::size_t {
  std::size_t committed = 0;
  for (std::size_t shipment_index = 0;
       shipment_index < shipments.size() &&
       shipment_index < result.chosen_candidate.size();
       ++shipment_index) {
    const auto chosen = result.chosen_candidate[shipment_index];
    if (!chosen.has_value()) {
      continue;
    }
    const auto& shipment = shipments[shipment_index];
    if (*chosen >= shipment.candidates.size()) {
      continue;
    }
    const auto& candidate = shipment.candidates[*chosen];
    if (capacity.reserve_for(
            shipment.id,
            std::span<const CapacityInstanceKey>{candidate.capacity_instances},
            candidate.units,
            shipment.reservation_options)
            .has_value()) {
      ++committed;
    }
  }
  for (const auto& [instance, price] : result.capacity_prices) {
    const auto smoothing = std::clamp(m_config.price_smoothing, 0.0, 1.0);
    const auto previous = capacity.price(instance);
    capacity.set_price(instance,
                       (previous * smoothing) +
                         (price * (1.0 - smoothing)));
  }
  return committed;
}

} // namespace moirai
