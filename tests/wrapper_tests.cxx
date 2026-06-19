#include "blocking_queue.hxx"
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
                  std::shared_ptr<PathCache> cache = nullptr) -> SolverWrapper {
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
  return SolverWrapper(queues, solver, timings_path, std::move(cache));
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
    iso_to_date("2026-06-08 12:00:00"),
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
    iso_to_date("2026-06-08 09:30:00"),
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
    iso_to_date("2026-06-08 12:00:00"),
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
    iso_to_date("2026-06-08 12:00:00"),
    packages);

  expect_eq(response.package_id, std::string{"child"},
            "package id comes from child");
  expect_true(!response.earliest.locations.empty(),
              "child critical returns earliest");
  expect_eq(response.is_critical, true, "child critical flag");
  expect_eq(response.ultimate.locations.empty(), true,
            "child critical omits ultimate");
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
          iso_to_date("2026-06-10 12:00:00"),
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

} // namespace

auto main() -> int {
  test_find_paths_non_critical_returns_earliest_and_ultimate();
  test_find_paths_critical_omits_ultimate();
  test_find_paths_missing_node_returns_fail();
  test_find_paths_child_can_make_parent_critical();
  test_find_paths_shared_cache_is_thread_safe();
  return 0;
}
