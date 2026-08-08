#include "blocking_queue.hxx"
#include <cstdlib>
#include <nlohmann/json.hpp>
#include "test_helpers.hxx"

import std;
import moirai.app;
import moirai.date_utils;
import moirai.search_document;
import moirai.solver;
import moirai.solver_wrapper;
import moirai.transportation;

namespace {

using moirai_tests::display_epoch_minutes;

class ScopedEnv {
private:
  std::string m_name;
  std::optional<std::string> m_previous;

public:
  ScopedEnv(std::string name, std::string value)
      : m_name(std::move(name)) {
    if (const char* previous = std::getenv(m_name.c_str()); previous != nullptr) {
      m_previous = previous;
    }
    if (::setenv(m_name.c_str(), value.c_str(), 1) != 0) {
      std::cerr << "failed to set env " << m_name << '\n';
      std::exit(1);
    }
  }

  ~ScopedEnv() {
    if (m_previous.has_value()) {
      (void)::setenv(m_name.c_str(), m_previous->c_str(), 1);
    } else {
      (void)::unsetenv(m_name.c_str());
    }
  }
};

template <typename T, typename U>
void expect_eq(const T& actual, const U& expected, std::string_view label,
               std::source_location location = std::source_location::current()) {
  if (actual == expected) {
    return;
  }

  std::cerr << location.file_name() << ':' << location.line() << ": " << label
            << " failed\n";
  std::exit(1);
}

void expect_true(bool value, std::string_view label,
                 std::source_location location =
                   std::source_location::current()) {
  expect_eq(value, true, label, location);
}

auto epoch_minutes(std::string_view timestamp) -> int32_t {
  return static_cast<int32_t>(iso_to_date(std::string(timestamp))
                                .time_since_epoch()
                                .count());
}

auto add_center(Solver& solver, std::string code) -> NodeId {
  auto center = std::make_shared<TransportCenter>(std::move(code));
  const auto node = solver.add_node(center);
  expect_true(node != INVALID_NODE, "center added");
  return node;
}

auto add_edge(Solver& solver, NodeId source, NodeId target,
              std::string code, int departure_minutes, int duration_minutes)
    -> void {
  const auto edge_id = solver.add_edge(
    source,
    target,
    std::make_shared<TransportEdge>(
      std::move(code),
      "route",
      DURATION{static_cast<std::int16_t>(departure_minutes)},
      DURATION{static_cast<std::int16_t>(duration_minutes)},
      DURATION{0},
      DURATION{0},
      VehicleType::SURFACE,
      MovementType::CARTING,
      false));
  expect_true(edge_id != INVALID_EDGE, "edge added");
}

auto make_wrapper(std::shared_ptr<Solver> solver,
                  std::shared_ptr<PathCache> cache = nullptr,
                  std::shared_ptr<CapacityReservationStore> capacity_store = nullptr)
    -> SolverWrapper {
  static const auto timings_path =
    std::filesystem::temp_directory_path() / "moirae-wrapper-empty-timings.json";
  static std::once_flag timings_once;
  std::call_once(timings_once, [] {
    std::ofstream timings(timings_path);
    timings << "[]";
  });

  static BlockingQueue<std::string> load_queue;
  static BlockingQueue<SearchDocument> solution_queue;
  SolverWrapper::RuntimeQueues queues{
    .node = nullptr,
    .edge = nullptr,
    .load = &load_queue,
    .solution = &solution_queue,
  };
  return SolverWrapper(queues,
                       solver,
                       timings_path,
                       std::move(cache),
                       nullptr,
                       std::move(capacity_store));
}

auto section_routes(const SearchPathSection& section) -> std::vector<std::string> {
  std::vector<std::string> routes;
  for (const auto& location : section.locations) {
    if (location.has_departure) {
      routes.push_back(location.route);
    }
  }
  return routes;
}

void test_find_paths_non_critical_returns_earliest_and_ultimate() {
  auto solver = std::make_shared<Solver>();
  const auto a = add_center(*solver, "A");
  const auto b = add_center(*solver, "B");
  add_edge(*solver, a, b, "A-B", 9 * 60, 60);
  auto wrapper = make_wrapper(solver);

  std::vector<std::tuple<std::string, int32_t, std::string>> packages;
  const auto response = wrapper.find_paths(
    "bag",
    "A",
    "B",
    epoch_minutes("2026-06-08 08:00:00"),
    DURATION{0},
    iso_to_date("2026-06-08 12:00:00"),
    DURATION{0},
    packages);

  expect_eq(response.waybill, std::string{"bag"}, "waybill copied");
  expect_eq(response.is_critical, false, "non-critical flag");
  expect_true(!response.earliest.locations.empty(), "earliest path present");
  expect_true(!response.ultimate.locations.empty(), "ultimate path present");
  expect_eq(response.earliest.locations.size(), std::size_t{2},
            "earliest has source and target");
  expect_eq(response.ultimate.locations.size(), std::size_t{2},
            "ultimate has source and target");
  expect_eq(response.pdd_ts,
            static_cast<std::int64_t>(
              iso_to_date("2026-06-08 12:00:00").time_since_epoch().count()) * 60,
            "pdd preserved");
  expect_eq(response.pdd_ts,
            static_cast<std::int64_t>(display_epoch_minutes(response.pdd)) * 60,
            "pdd_ts matches pdd display");
}

void test_find_paths_critical_omits_ultimate() {
  auto solver = std::make_shared<Solver>();
  const auto a = add_center(*solver, "A");
  const auto b = add_center(*solver, "B");
  add_edge(*solver, a, b, "A-B", 9 * 60, 60);
  auto wrapper = make_wrapper(solver);

  std::vector<std::tuple<std::string, int32_t, std::string>> packages;
  const auto response = wrapper.find_paths(
    "bag",
    "A",
    "B",
    epoch_minutes("2026-06-08 08:00:00"),
    DURATION{0},
    iso_to_date("2026-06-08 09:30:00"),
    DURATION{0},
    packages);

  expect_true(!response.earliest.locations.empty(),
              "critical still returns earliest");
  expect_eq(response.is_critical, true, "critical flag");
  expect_eq(response.ultimate.locations.empty(), true,
            "critical path omits ultimate");
}

void test_find_paths_missing_node_returns_fail() {
  auto solver = std::make_shared<Solver>();
  add_center(*solver, "A");
  auto wrapper = make_wrapper(solver);
  moirai::Application::instance().logger().set_level("debug");
  moirai_tests::ScopedLogCapture logs;

  std::vector<std::tuple<std::string, int32_t, std::string>> packages;
  const auto response = wrapper.find_paths(
    "bag",
    "A",
    "missing",
    epoch_minutes("2026-06-08 08:00:00"),
    DURATION{0},
    iso_to_date("2026-06-08 12:00:00"),
    DURATION{0},
    packages);

  expect_true(!response.fail.empty(), "missing node returns fail");
  expect_eq(response.is_critical, true, "missing node critical flag");
  expect_true(logs.contains("Pathing failed"), "missing node debug log");
  moirai::Application::instance().logger().set_level("information");
}

void test_find_paths_child_can_make_parent_critical() {
  auto solver = std::make_shared<Solver>();
  const auto a = add_center(*solver, "A");
  const auto b = add_center(*solver, "B");
  const auto c = add_center(*solver, "C");
  add_edge(*solver, a, b, "A-B", 9 * 60, 60);
  add_edge(*solver, b, c, "B-C", (9 * 60) + 30, 60);
  auto wrapper = make_wrapper(solver);

  std::vector<std::tuple<std::string, int32_t, std::string>> packages{
    {"C", epoch_minutes("2026-06-08 10:30:00"), "child"}
  };
  const auto response = wrapper.find_paths(
    "bag",
    "A",
    "B",
    epoch_minutes("2026-06-08 08:00:00"),
    DURATION{0},
    iso_to_date("2026-06-08 12:00:00"),
    DURATION{0},
    packages);

  expect_eq(response.package_id, std::string{"child"},
            "package id comes from child");
  expect_true(!response.earliest.locations.empty(),
              "child critical returns earliest");
  expect_eq(response.is_critical, true, "child critical flag");
  expect_eq(response.ultimate.locations.empty(), true,
            "child critical omits ultimate");
}

void test_find_paths_source_processing_offset_can_make_critical() {
  auto solver = std::make_shared<Solver>();
  const auto a = add_center(*solver, "A");
  const auto b = add_center(*solver, "B");
  add_edge(*solver, a, b, "A-B", 9 * 60, 60);
  auto wrapper = make_wrapper(solver);

  std::vector<std::tuple<std::string, int32_t, std::string>> packages;
  const auto no_offset = wrapper.find_paths(
    "bag-no-offset",
    "A",
    "B",
    epoch_minutes("2026-06-08 08:45:00"),
    DURATION{0},
    iso_to_date("2026-06-08 12:00:00"),
    DURATION{0},
    packages);
  expect_eq(no_offset.is_critical, false,
            "load catches route before source processing offset");

  const auto fresh_offset = wrapper.find_paths(
    "bag-fresh-offset",
    "A",
    "B",
    epoch_minutes("2026-06-08 08:45:00"),
    DURATION{30},
    iso_to_date("2026-06-08 12:00:00"),
    DURATION{0},
    packages);
  expect_eq(fresh_offset.is_critical, true,
            "fresh processing offset can miss the current departure");
}

void test_find_paths_mixed_bag_processing_can_make_parent_critical() {
  auto solver = std::make_shared<Solver>();
  const auto a = add_center(*solver, "A");
  const auto b = add_center(*solver, "B");
  const auto c = add_center(*solver, "C");
  add_edge(*solver, a, b, "A-B", 9 * 60, 60);
  add_edge(*solver, b, c, "B-C", (10 * 60) + 15, 15);
  auto wrapper = make_wrapper(solver);

  std::vector<std::tuple<std::string, int32_t, std::string>> packages{
    {"B", epoch_minutes("2026-06-08 10:05:00"), "same-destination-child"},
    {"C", epoch_minutes("2026-06-08 10:30:00"), "child"}
  };
  const auto without_mixed_processing = wrapper.find_paths(
    "bag-without-mixed",
    "A",
    "B",
    epoch_minutes("2026-06-08 08:00:00"),
    DURATION{0},
    iso_to_date("2026-06-08 12:00:00"),
    DURATION{0},
    packages);
  expect_eq(without_mixed_processing.is_critical, false,
            "child path leaves enough time without mixed bag processing");

  const auto with_mixed_processing = wrapper.find_paths(
    "bag-with-mixed",
    "A",
    "B",
    epoch_minutes("2026-06-08 08:00:00"),
    DURATION{0},
    iso_to_date("2026-06-08 12:00:00"),
    DURATION{30},
    packages);
  expect_eq(with_mixed_processing.is_critical, true,
            "mixed bag processing can consume parent slack");
}

void test_find_paths_shared_cache_is_thread_safe() {
  auto solver = std::make_shared<Solver>();
  std::vector<NodeId> nodes;
  nodes.reserve(32);
  for (int index = 0; index < 32; ++index) {
    nodes.push_back(add_center(*solver, std::format("N{}", index)));
  }
  for (int index = 0; index < 31; ++index) {
    add_edge(*solver,
             nodes[static_cast<std::size_t>(index)],
             nodes[static_cast<std::size_t>(index + 1)],
             std::format("N{}-N{}", index, index + 1),
             (8 * 60) + index,
             30);
    if (index + 2 < 32) {
      add_edge(*solver,
               nodes[static_cast<std::size_t>(index)],
               nodes[static_cast<std::size_t>(index + 2)],
               std::format("N{}-N{}", index, index + 2),
               (9 * 60) + index,
               45);
    }
  }
  solver->finalize_graph();

  auto cache = std::make_shared<PathCache>(256);
  constexpr int worker_count = 8;
  constexpr int iterations = 200;
  std::vector<std::jthread> workers;
  std::atomic<int> failures{0};
  workers.reserve(worker_count);

  for (int worker = 0; worker < worker_count; ++worker) {
    workers.emplace_back([solver, cache, worker, &failures]() {
      auto wrapper = make_wrapper(solver, cache);
      std::vector<std::tuple<std::string, int32_t, std::string>> packages;
      packages.emplace_back("N31",
                            epoch_minutes("2026-06-08 18:00:00"),
                            std::format("child-{}", worker));
      for (int iteration = 0; iteration < iterations; ++iteration) {
        const auto source_index = iteration % 8;
        const auto target_index = 24 + (iteration % 8);
        const auto response = wrapper.find_paths(
          std::format("bag-{}-{}", worker, iteration),
          std::format("N{}", source_index),
          std::format("N{}", target_index),
          epoch_minutes("2026-06-08 07:00:00") + (iteration % 3),
          DURATION{0},
          iso_to_date("2026-06-10 12:00:00"),
          DURATION{0},
          packages);
        if (response.failed() || response.earliest.locations.empty()) {
          failures.fetch_add(1, std::memory_order_relaxed);
        }
      }
    });
  }

  workers.clear();
  expect_eq(failures.load(), 0, "shared path cache concurrent failures");
  const auto metrics = cache->metrics();
  expect_true(metrics.hits > 0, "shared path cache records hits");
  expect_true(metrics.misses > 0, "shared path cache records misses");
}

void test_capacity_reservation_routes_around_full_departure() {
  const ScopedEnv enabled{"MOIRAI_CAPACITY_ENABLED", "true"};
  const ScopedEnv missing{"MOIRAI_CAPACITY_MISSING_ALLOWED", "false"};

  auto solver = std::make_shared<Solver>();
  const auto a = add_center(*solver, "A");
  const auto b = add_center(*solver, "B");
  const auto c = add_center(*solver, "C");
  add_edge(*solver, a, b, "A-B", 9 * 60, 60);
  add_edge(*solver, a, c, "A-C", 9 * 60, 30);
  add_edge(*solver, c, b, "C-B", 10 * 60, 30);

  auto capacity = std::make_shared<CapacityReservationStore>();
  const auto direct = solver->find_edge("A-B");
  const auto first_leg = solver->find_edge("A-C");
  const auto second_leg = solver->find_edge("C-B");
  expect_true(direct.has_value(), "direct edge exists");
  expect_true(first_leg.has_value(), "first alternate edge exists");
  expect_true(second_leg.has_value(), "second alternate edge exists");
  capacity->set_capacity(
    CapacityInstanceKey{.edge = *direct,
                        .departure = static_cast<SolverMinute>(
                          epoch_minutes("2026-06-08 09:00:00"))},
    1);
  capacity->set_capacity(
    CapacityInstanceKey{.edge = *first_leg,
                        .departure = static_cast<SolverMinute>(
                          epoch_minutes("2026-06-08 09:00:00"))},
    10);
  capacity->set_capacity(
    CapacityInstanceKey{.edge = *second_leg,
                        .departure = static_cast<SolverMinute>(
                          epoch_minutes("2026-06-08 10:00:00"))},
    10);

  auto wrapper = make_wrapper(solver, nullptr, capacity);
  std::vector<std::tuple<std::string, int32_t, std::string>> packages;
  const auto first = wrapper.find_paths(
    "bag-1",
    "A",
    "B",
    epoch_minutes("2026-06-08 08:00:00"),
    DURATION{0},
    iso_to_date("2026-06-08 12:00:00"),
    DURATION{0},
    packages);
  expect_eq(section_routes(first.earliest),
            std::vector<std::string>({"A-B"}),
            "first capacity-aware shipment uses direct edge");

  const auto second = wrapper.find_paths(
    "bag-2",
    "A",
    "B",
    epoch_minutes("2026-06-08 08:00:00"),
    DURATION{0},
    iso_to_date("2026-06-08 12:00:00"),
    DURATION{0},
    packages);
  expect_eq(section_routes(second.earliest),
            std::vector<std::string>({"A-C", "C-B"}),
            "second capacity-aware shipment avoids reserved direct edge");
  const auto remaining = capacity->remaining(
    CapacityInstanceKey{.edge = *direct,
                        .departure = static_cast<SolverMinute>(
                          epoch_minutes("2026-06-08 09:00:00"))});
  expect_true(remaining.has_value(), "direct remaining capacity exists");
  expect_eq(*remaining, CapacityUnits{0}, "direct capacity exhausted");
}

void test_capacity_search_waits_for_later_departure_instance() {
  const ScopedEnv enabled{"MOIRAI_CAPACITY_ENABLED", "true"};
  const ScopedEnv missing{"MOIRAI_CAPACITY_MISSING_ALLOWED", "false"};

  auto solver = std::make_shared<Solver>();
  const auto a = add_center(*solver, "A");
  const auto b = add_center(*solver, "B");
  add_edge(*solver, a, b, "A-B", 9 * 60, 60);

  auto capacity = std::make_shared<CapacityReservationStore>();
  const auto edge = solver->find_edge("A-B");
  expect_true(edge.has_value(), "edge exists");
  capacity->set_capacity(
    CapacityInstanceKey{.edge = *edge,
                        .departure = static_cast<SolverMinute>(
                          epoch_minutes("2026-06-08 09:00:00"))},
    0);
  capacity->set_capacity(
    CapacityInstanceKey{.edge = *edge,
                        .departure = static_cast<SolverMinute>(
                          epoch_minutes("2026-06-09 09:00:00"))},
    1);

  auto wrapper = make_wrapper(solver, nullptr, capacity);
  std::vector<std::tuple<std::string, int32_t, std::string>> packages;
  const auto response = wrapper.find_paths(
    "bag",
    "A",
    "B",
    epoch_minutes("2026-06-08 08:00:00"),
    DURATION{0},
    iso_to_date("2026-06-10 12:00:00"),
    DURATION{0},
    packages);

  expect_true(!response.earliest.locations.empty(),
              "later capacity instance keeps path feasible");
  expect_eq(response.earliest.locations.front().departure_ts,
            static_cast<std::int64_t>(
              epoch_minutes("2026-06-09 09:00:00")) * 60,
            "capacity search selects later departure instance");
}

void test_rolling_horizon_runtime_selects_capacity_feasible_candidates() {
  const ScopedEnv enabled{"MOIRAI_CAPACITY_ENABLED", "true"};
  const ScopedEnv missing{"MOIRAI_CAPACITY_MISSING_ALLOWED", "false"};
  const ScopedEnv candidates{"MOIRAI_CAPACITY_CANDIDATE_COUNT", "2"};
  const ScopedEnv rolling{"MOIRAI_ROLLING_HORIZON_ENABLED", "true"};
  const ScopedEnv highs{"MOIRAI_ROLLING_HORIZON_USE_HIGHS", "false"};
  const ScopedEnv max_batch{"MOIRAI_ROLLING_HORIZON_MAX_BATCH_SIZE", "2"};
  const ScopedEnv window{"MOIRAI_ROLLING_HORIZON_WINDOW_MS", "25"};

  auto solver = std::make_shared<Solver>();
  const auto a = add_center(*solver, "A");
  const auto b = add_center(*solver, "B");
  const auto c = add_center(*solver, "C");
  add_edge(*solver, a, b, "A-B", 9 * 60, 60);
  add_edge(*solver, a, c, "A-C", 9 * 60, 30);
  add_edge(*solver, c, b, "C-B", 11 * 60, 30);

  auto capacity = std::make_shared<CapacityReservationStore>();
  const auto direct = solver->find_edge("A-B");
  const auto first_leg = solver->find_edge("A-C");
  const auto second_leg = solver->find_edge("C-B");
  expect_true(direct.has_value(), "direct edge exists");
  expect_true(first_leg.has_value(), "alternate first edge exists");
  expect_true(second_leg.has_value(), "alternate second edge exists");
  capacity->set_capacity(
    CapacityInstanceKey{.edge = *direct,
                        .departure = static_cast<SolverMinute>(
                          epoch_minutes("2026-06-08 09:00:00"))},
    1);
  capacity->set_capacity(
    CapacityInstanceKey{.edge = *first_leg,
                        .departure = static_cast<SolverMinute>(
                          epoch_minutes("2026-06-08 09:00:00"))},
    10);
  capacity->set_capacity(
    CapacityInstanceKey{.edge = *second_leg,
	                        .departure = static_cast<SolverMinute>(
	                          epoch_minutes("2026-06-08 11:00:00"))},
	    10);

  BlockingQueue<std::string> load_queue;
  BlockingQueue<SearchDocument> solution_queue;
  SolverWrapper::RuntimeQueues queues{
    .node = nullptr,
    .edge = nullptr,
    .load = &load_queue,
    .solution = &solution_queue,
  };
  static const auto timings_path =
    std::filesystem::temp_directory_path() /
    "moirae-wrapper-rolling-empty-timings.json";
  {
    std::ofstream timings(timings_path);
    timings << "[]";
  }
  auto profiles = std::make_shared<SolverWrapper::FacilityProfiles>();
  (*profiles)["B"].center_arrival_cutoff = DURATION{10 * 60 + 30};
  SolverWrapper wrapper{
    queues,
    solver,
    timings_path,
    nullptr,
    profiles,
    capacity,
  };

  const auto load = [](std::string id, const bool constrained) {
    auto payload = nlohmann::json{
      {"id", std::move(id)},
      {"location", "A"},
      {"destination", "B"},
      {"time", "2026-06-08 08:00:00"},
      {"capacity_units", 1},
    };
    if (constrained) {
      payload["ipdd_destination"] = "2026-06-08 00:00:00";
    }
    return payload.dump();
  };
  expect_true(load_queue.enqueue(load("bag-1", false)), "first load enqueued");
  expect_true(load_queue.enqueue(load("bag-2", true)), "second load enqueued");
  load_queue.close();
  wrapper.run(std::stop_token{});

  std::array<SearchDocument, 2> results;
  const auto count = solution_queue.try_dequeue_bulk(results.data(),
                                                    results.size());
  expect_eq(count, std::size_t{2}, "rolling horizon emits two documents");
  expect_eq(section_routes(results[0].earliest),
            std::vector<std::string>({"A-C", "C-B"}),
            "unconstrained rolling output yields scarce direct capacity");
  expect_eq(section_routes(results[1].earliest),
            std::vector<std::string>({"A-B"}),
            "SLA-constrained rolling output receives direct route");
  expect_true(capacity->reservation("bag-1").has_value(),
              "first rolling result is reserved");
  expect_true(capacity->reservation("bag-2").has_value(),
              "second rolling result is reserved");
}

void test_capacity_lifecycle_events_are_idempotent_and_order_tolerant() {
  const ScopedEnv enabled{"MOIRAI_CAPACITY_ENABLED", "true"};
  const ScopedEnv missing{"MOIRAI_CAPACITY_MISSING_ALLOWED", "false"};

  auto solver = std::make_shared<Solver>();
  const auto a = add_center(*solver, "A");
  const auto b = add_center(*solver, "B");
  add_edge(*solver, a, b, "A-B", 9 * 60, 60);

  auto capacity = std::make_shared<CapacityReservationStore>();
  const auto edge = solver->find_edge("A-B");
  expect_true(edge.has_value(), "edge exists");
  capacity->set_capacity(
    CapacityInstanceKey{.edge = *edge,
                        .departure = static_cast<SolverMinute>(
                          epoch_minutes("2026-06-08 09:00:00"))},
    1);

  BlockingQueue<std::string> load_queue;
  BlockingQueue<SearchDocument> solution_queue;
  SolverWrapper::RuntimeQueues queues{
    .node = nullptr,
    .edge = nullptr,
    .load = &load_queue,
    .solution = &solution_queue,
  };
  static const auto timings_path =
    std::filesystem::temp_directory_path() /
    "moirae-wrapper-lifecycle-empty-timings.json";
  {
    std::ofstream timings(timings_path);
    timings << "[]";
  }
  SolverWrapper wrapper{
    queues,
    solver,
    timings_path,
    nullptr,
    nullptr,
    capacity,
  };

  const auto load = nlohmann::json{
    {"id", "bag-lifecycle"},
    {"location", "A"},
    {"destination", "B"},
    {"time", "2026-06-08 08:00:00"},
    {"capacity_units", 1},
  }.dump();
  const auto release = nlohmann::json{
    {"capacity_action", "release"},
    {"owner", "bag-lifecycle"},
  }.dump();
  const auto consume = nlohmann::json{
    {"capacity_action", "consume"},
    {"owner", "bag-lifecycle"},
  }.dump();

  expect_true(load_queue.enqueue(release),
              "out-of-order release enqueued");
  expect_true(load_queue.enqueue(load), "load enqueued");
  expect_true(load_queue.enqueue(load), "duplicate load enqueued");
  expect_true(load_queue.enqueue(consume), "consume enqueued");
  expect_true(load_queue.enqueue(consume), "duplicate consume enqueued");
  load_queue.close();
  wrapper.run(std::stop_token{});

  std::array<SearchDocument, 2> results;
  const auto count = solution_queue.try_dequeue_bulk(results.data(),
                                                    results.size());
  expect_eq(count, std::size_t{2},
            "duplicate load emits duplicate path recommendations");
  expect_true(!capacity->reservation("bag-lifecycle").has_value(),
              "consume clears lifecycle reservation");
  expect_eq(capacity->remaining(CapacityInstanceKey{
              .edge = *edge,
              .departure = static_cast<SolverMinute>(
                epoch_minutes("2026-06-08 09:00:00"))})
              .value_or(99),
            CapacityUnits{0},
            "consumed capacity remains unavailable after duplicate events");
}

} // namespace

auto main() -> int {
  test_find_paths_non_critical_returns_earliest_and_ultimate();
  test_find_paths_critical_omits_ultimate();
  test_find_paths_missing_node_returns_fail();
  test_find_paths_child_can_make_parent_critical();
  test_find_paths_source_processing_offset_can_make_critical();
  test_find_paths_mixed_bag_processing_can_make_parent_critical();
  test_find_paths_shared_cache_is_thread_safe();
  test_capacity_reservation_routes_around_full_departure();
  test_capacity_search_waits_for_later_departure_instance();
  test_rolling_horizon_runtime_selects_capacity_feasible_candidates();
  test_capacity_lifecycle_events_are_idempotent_and_order_tolerant();
  return 0;
}
