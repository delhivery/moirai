#include "blocking_queue.hxx"
#include "date_utils.hxx"
#include "solver.hxx"
#include "solver_wrapper.hxx"
#include "test_helpers.hxx"
#include "transportation.hxx"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

template <typename T, typename U>
void expect_eq(const T& actual, const U& expected, std::string_view label,
               std::source_location location = std::source_location::current()) {
  if (actual == expected) {
    return;
  }

  std::cerr << location.file_name() << ':' << location.line() << ": " << label
            << " failed\n";
  std::exit(EXIT_FAILURE);
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

auto add_center(Solver& solver, std::string code) -> Node<Graph> {
  auto center = std::make_shared<TransportCenter>(std::move(code));
  const auto [node, inserted] = solver.add_node(center);
  expect_true(inserted, "center added");
  return node;
}

auto add_edge(Solver& solver, Node<Graph> source, Node<Graph> target,
              std::string code, int departure_minutes, int duration_minutes)
    -> void {
  solver.add_edge(
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
}

auto make_wrapper(std::shared_ptr<Solver> solver) -> SolverWrapper {
  static const auto timings_path =
    std::filesystem::temp_directory_path() / "moirae-wrapper-empty-timings.json";
  {
    std::ofstream timings(timings_path);
    timings << "[]";
  }

  static BlockingQueue<std::string> load_queue;
  static BlockingQueue<std::string> solution_queue;
  SolverWrapper::RuntimeQueues queues{
    .node = nullptr,
    .edge = nullptr,
    .load = &load_queue,
    .solution = &solution_queue,
  };
  return SolverWrapper(queues, solver, timings_path);
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

  expect_eq(response["waybill"].get<std::string>(), std::string{"bag"},
            "waybill copied");
  expect_true(response.contains("earliest"), "earliest path present");
  expect_true(response.contains("ultimate"), "ultimate path present");
  expect_eq(response["earliest"]["locations"].size(), size_t{2},
            "earliest has source and target");
  expect_eq(response["ultimate"]["locations"].size(), size_t{2},
            "ultimate has source and target");
  expect_eq(response["pdd_ts"].get<std::uint32_t>(),
            iso_to_date("2026-06-08 12:00:00").time_since_epoch().count(),
            "pdd preserved");
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

  expect_true(response.contains("earliest"), "critical still returns earliest");
  expect_eq(response.contains("ultimate"), false,
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

  expect_true(response.contains("fail"), "missing node returns fail");
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

  expect_eq(response["package"].get<std::string>(), std::string{"child"},
            "package id comes from child");
  expect_true(response.contains("earliest"), "child critical returns earliest");
  expect_eq(response.contains("ultimate"), false,
            "child critical omits ultimate");
}

} // namespace

auto main() -> int {
  test_find_paths_non_critical_returns_earliest_and_ultimate();
  test_find_paths_critical_omits_ultimate();
  test_find_paths_missing_node_returns_fail();
  test_find_paths_child_can_make_parent_critical();
  return EXIT_SUCCESS;
}
