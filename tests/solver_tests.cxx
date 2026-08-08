#include <nlohmann/json.hpp>

import std;
import moirai.capacity_optimizer;
import moirai.date_utils;
import moirai.json_utils;
import moirai.route_schedule;
import moirai.solver;
import moirai.transportation;

#ifndef MOIRAI_TEST_FIXTURE_DIR
#define MOIRAI_TEST_FIXTURE_DIR "tests/fixtures"
#endif

namespace {

constexpr auto IST_OFFSET = DURATION{330};

// Wrapper that owns a parser + the parsed element.
// Each instance keeps its document alive independently.
struct TestDoc {
  std::unique_ptr<moirai::JsonParser> parser =
    std::make_unique<moirai::JsonParser>();
  std::string source;
  moirai::Json element{};

  TestDoc() = default;
  TestDoc(const TestDoc&) = delete;
  auto operator=(const TestDoc&) -> TestDoc& = delete;
  TestDoc(TestDoc&&) = default;
  auto operator=(TestDoc&&) -> TestDoc& = default;

  static auto from(const nlohmann::json& json) -> TestDoc {
    TestDoc doc;
    doc.source = json.dump();
    auto parsed = moirai::parse_json(*doc.parser, doc.source);
    if (!parsed.has_value()) {
      std::cerr << "TestDoc parse failed\n";
      std::exit(1);
    }
    doc.element = *parsed;
    return doc;
  }

  static auto from_string(std::string json_str) -> TestDoc {
    TestDoc doc;
    doc.source = std::move(json_str);
    auto parsed = moirai::parse_json(*doc.parser, doc.source);
    if (!parsed.has_value()) {
      std::cerr << "TestDoc parse failed\n";
      std::exit(1);
    }
    doc.element = *parsed;
    return doc;
  }

  operator const moirai::Json&() const { return element; }
};

auto day_mask(int day) -> std::uint8_t {
  return static_cast<std::uint8_t>(1U << (((day % 7) + 7) % 7));
}

auto minutes_between(CLOCK lhs, CLOCK rhs) -> int {
  return static_cast<int>((lhs - rhs).count());
}

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

void expect_not_empty(const Path& path,
                      std::string_view label,
                      std::source_location location =
                        std::source_location::current()) {
  if (!path.empty()) {
    return;
  }

  std::cerr << location.file_name() << ':' << location.line() << ": " << label
            << " failed\n";
  std::exit(1);
}

struct GraphBuilder {
  Solver solver;

  auto add_center(std::string code) -> NodeId {
    auto center = std::make_shared<TransportCenter>(std::move(code));
    const auto node = solver.add_node(center);
    expect_true(node != INVALID_NODE, "center insertion");
    return node;
  }

  auto add_edge(NodeId source, NodeId target, std::string code,
                int departure_minutes, int duration_minutes,
                std::uint8_t days = ALL_DAYS_OF_WEEK,
                VehicleType vehicle = VehicleType::SURFACE,
                MovementType movement = MovementType::CARTING,
                bool terminal = false) -> std::shared_ptr<TransportEdge> {
    auto edge = std::make_shared<TransportEdge>(
      std::move(code),
      "route",
      DURATION{static_cast<std::int16_t>(departure_minutes)},
      DURATION{static_cast<std::int16_t>(duration_minutes)},
      DURATION{0},
      DURATION{0},
      vehicle,
      movement,
      terminal,
      days);
    expect_true(solver.add_edge(source, target, edge) != INVALID_EDGE,
                "edge insertion");
    return edge;
  }
};

auto node_codes(const Path& path)
    -> std::vector<std::string> {
  std::vector<std::string> codes;
  codes.reserve(path.steps.size());
  for (const auto& step : path.steps) {
    codes.push_back(step.node->code);
  }
  return codes;
}

auto edge_codes(const Path& path)
    -> std::vector<std::string> {
  std::vector<std::string> codes;
  codes.reserve(path.steps.size());
  for (const auto& step : path.steps) {
    if (step.outbound != nullptr) {
      codes.push_back(step.outbound->code);
    }
  }
  return codes;
}

auto last_step(const Path& path) -> const PathStep& {
  return path.back();
}

auto make_base_route_json() -> nlohmann::json {
  return {
    {"route_schedule_uuid", "route"},
    {"name", "Test Route"},
    {"route_type", "AIR"},
    {"reporting_time", "06:00"},
    {"days_of_week", {1, 2, 3, 4, 5, 6}},
    {"halt_centers",
     {
       {{"center_code", "A"},
        {"rel_eta", "0:00"},
        {"rel_etd", "1:00"},
        {"loading_allowed", true}},
       {{"center_code", "B"},
        {"rel_eta", "2:00"},
        {"rel_etd", "2:30"},
        {"loading_allowed", false}},
       {{"center_code", "C"},
        {"rel_eta", "3:00"},
        {"rel_etd", "3:30"},
        {"loading_allowed", true}},
       {{"center_code", "D"},
        {"rel_eta", "5:00"},
        {"rel_etd", "5:45"}},
     }},
  };
}

auto make_base_route() -> TestDoc {
  return TestDoc::from(make_base_route_json());
}

auto load_json_fixture(std::string_view name) -> TestDoc {
  const auto path = std::filesystem::path{MOIRAI_TEST_FIXTURE_DIR} / name;
  std::ifstream input(path);
  if (!input.is_open()) {
    std::cerr << "Failed to open fixture " << path << '\n';
    std::exit(1);
  }

  std::string content(std::istreambuf_iterator<char>(input), {});
  return TestDoc::from_string(std::move(content));
}

auto loading_stop_count(const moirai::Json& route) -> std::size_t {
  const auto* stops = moirai::find_array_member(route, "halt_centers");
  if (stops == nullptr) {
    return 0;
  }

  std::size_t count = 0;
  for (const auto& stop : *stops) {
    const auto* loading_allowed = moirai::find_member(stop, "loading_allowed");
    const auto bool_val = loading_allowed != nullptr
                            ? moirai::get_bool(*loading_allowed)
                            : std::nullopt;
    if (!bool_val.has_value() || *bool_val) {
      ++count;
    }
  }
  return count;
}

auto has_blocked_stop(const moirai::Json& route) -> bool {
  const auto* stops = moirai::find_array_member(route, "halt_centers");
  if (stops == nullptr) {
    return false;
  }

  for (const auto& stop : *stops) {
    const auto* loading_allowed = moirai::find_member(stop, "loading_allowed");
    if (loading_allowed != nullptr) {
      const auto bool_val = moirai::get_bool(*loading_allowed);
      if (bool_val.has_value() && !*bool_val) {
        return true;
      }
    }
  }
  return false;
}

auto has_day_prefixed_time(const moirai::Json& route) -> bool {
  const auto* stops = moirai::find_array_member(route, "halt_centers");
  if (stops == nullptr) {
    return false;
  }

  for (const auto& stop : *stops) {
    for (const auto* key : {"rel_eta", "rel_etd"}) {
      const auto value = moirai::find_string_member(stop, key);
      if (value.has_value() && value->find("day,") != std::string_view::npos) {
        return true;
      }
    }
  }
  return false;
}

auto blocked_center_codes(const moirai::Json& route) -> std::vector<std::string> {
  std::vector<std::string> codes;
  const auto* stops = moirai::find_array_member(route, "halt_centers");
  if (stops == nullptr) {
    return codes;
  }

  for (const auto& stop : *stops) {
    const auto* loading_allowed = moirai::find_member(stop, "loading_allowed");
    const auto center_code = moirai::find_string_member(stop, "center_code");
    if (loading_allowed != nullptr) {
      const auto bool_val = moirai::get_bool(*loading_allowed);
      if (bool_val.has_value() && !*bool_val && center_code.has_value()) {
        codes.emplace_back(*center_code);
      }
    }
  }
  return codes;
}

void test_graph_basics() {
  GraphBuilder graph;
  const auto a = graph.add_center("A");
  const auto b = graph.add_center("B");

  const auto existing_a = graph.solver.find_node("A");
  expect_true(existing_a.has_value(), "existing node lookup succeeds");
  expect_eq(*existing_a, a, "existing node descriptor is reused");

  const auto missing = graph.solver.find_node("missing");
  expect_eq(missing.has_value(), false, "missing node lookup fails");
  expect_eq(graph.solver.get_node(INVALID_NODE) == nullptr, true,
            "invalid node lookup returns null");
  expect_eq(graph.solver.add_node(nullptr), INVALID_NODE,
            "null center insertion returns invalid node");

  graph.add_edge(a, b, "edge", 9 * 60, 60);
  const auto existing_edge = graph.solver.find_edge("edge");
  expect_true(existing_edge.has_value(), "existing edge lookup succeeds");

  auto duplicate = std::make_shared<TransportEdge>(
    "edge", "duplicate", DURATION{600}, DURATION{60}, DURATION{0},
    DURATION{0}, VehicleType::SURFACE, MovementType::CARTING, false);
  const auto duplicate_edge =
    graph.solver.add_edge(a, b, duplicate);
  expect_eq(duplicate_edge, *existing_edge,
            "duplicate edge descriptor reused");
  expect_eq(graph.solver.find_edge("missing").has_value(), false,
            "missing edge lookup fails");
  expect_eq(graph.solver.add_edge(INVALID_NODE, b, duplicate), INVALID_EDGE,
            "invalid source edge insertion returns invalid edge");
  expect_eq(graph.solver.add_edge(a, INVALID_NODE, duplicate), INVALID_EDGE,
            "invalid target edge insertion returns invalid edge");
  expect_eq(graph.solver.add_edge(a, b, nullptr), INVALID_EDGE,
            "null edge insertion returns invalid edge");
  const auto start = iso_to_date("2026-06-08 08:00:00");
  const auto invalid_source_path =
    graph.solver.find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
      INVALID_NODE, b, start);
  expect_eq(invalid_source_path.empty(), true,
            "invalid source path lookup returns empty path");
  const auto invalid_target_path =
    graph.solver.find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
      a, INVALID_NODE, start);
  expect_eq(invalid_target_path.empty(), true,
            "invalid target path lookup returns empty path");
  expect_eq(graph.solver.show(), std::string{"Graph<2, 1>"},
            "duplicate edge does not change graph size");

  const auto graph_dump = graph.solver.show_all();
  expect_true(graph_dump.find("A") != std::string::npos,
              "show_all contains source node");
  expect_true(graph_dump.find("edge: A TO B") != std::string::npos,
              "show_all contains edge");
}

void test_value_overloads_and_csr_invalidation() {
  Solver solver;
  auto source_center = TransportCenter{"A"};
  auto target_center = TransportCenter{"B"};
  source_center.set_latency<MovementType::CARTING, ProcessType::OUTBOUND>(
      DURATION{10});
  target_center.set_latency<MovementType::CARTING, ProcessType::INBOUND>(
      DURATION{5});
  const auto a = solver.add_node(std::move(source_center));
  const auto b = solver.add_node(std::move(target_center));
  expect_true(a != INVALID_NODE && b != INVALID_NODE,
              "value center insertion succeeds");

  auto edge = TransportEdge("A-B", "route", DURATION{9 * 60}, DURATION{30},
                            DURATION{0}, DURATION{0}, VehicleType::SURFACE,
                            MovementType::CARTING, false);
  expect_true(solver.add_edge(a, b, std::move(edge)) != INVALID_EDGE,
              "value edge insertion succeeds");
  solver.finalize_graph();

  const auto start = iso_to_date("2026-06-08 08:00:00");
  const auto path =
    solver.find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
      a, b, start);
  expect_not_empty(path, "explicitly finalized CSR path exists");
  expect_eq(node_codes(path), std::vector<std::string>({"A", "B"}),
            "value overload path node order");

  const auto c = solver.add_node(TransportCenter{"C"});
  auto next = TransportEdge("B-C", "route", DURATION{10 * 60}, DURATION{15},
                            DURATION{0}, DURATION{0}, VehicleType::SURFACE,
                            MovementType::CARTING, false);
  expect_true(solver.add_edge(b, c, std::move(next)) != INVALID_EDGE,
              "post-finalize edge insertion succeeds");
  const auto invalidated_path =
    solver.find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
      a, c, start);
  expect_not_empty(invalidated_path, "lazy CSR rebuild path exists");
  expect_eq(node_codes(invalidated_path),
            std::vector<std::string>({"A", "B", "C"}),
            "CSR invalidation includes post-finalize edge");

  const auto isolated = solver.add_node(TransportCenter{"D"});
  const auto missing_path =
    solver.find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
      isolated, a, start);
  expect_eq(missing_path.empty(), true, "isolated node has empty adjacency");
}

void test_concurrent_lazy_csr_rebuild_is_thread_safe() {
  GraphBuilder graph;
  constexpr std::size_t node_count = 256;
  std::vector<NodeId> nodes;
  nodes.reserve(node_count);
  for (std::size_t index = 0; index < node_count; ++index) {
    nodes.push_back(graph.add_center(std::format("N{}", index)));
  }

  for (std::size_t index = 0; index + 1 < node_count; ++index) {
    graph.add_edge(nodes[index],
                   nodes[index + 1],
                   std::format("E{}", index),
                   0,
                   0);
  }
  graph.add_edge(nodes.front(), nodes.back(), "direct", 0, 1);

  constexpr std::size_t worker_count = 16;
  constexpr std::size_t iterations = 100;
  std::barrier start_line(worker_count + 1);
  std::atomic_bool failed{false};
  std::vector<std::jthread> workers;
  workers.reserve(worker_count);
  const auto start = iso_to_date("2026-06-08 00:00:00");

  for (std::size_t worker = 0; worker < worker_count; ++worker) {
    workers.emplace_back([&]() {
      start_line.arrive_and_wait();
      for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        const auto path =
          graph.solver
            .find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
              nodes.front(), nodes.back(), start);
        if (path.empty() || path.front().node->code != "N0" ||
            path.back().node->code != std::format("N{}", node_count - 1)) {
          failed.store(true, std::memory_order_relaxed);
          return;
        }
      }
    });
  }

  start_line.arrive_and_wait();
  workers.clear();
  expect_eq(failed.load(std::memory_order_relaxed), false,
            "concurrent lazy CSR rebuild returns stable paths");
}

void test_forward_path_selection() {
  GraphBuilder graph;
  const auto a = graph.add_center("A");
  const auto b = graph.add_center("B");
  const auto c = graph.add_center("C");

  graph.add_edge(a, b, "A-B", (8 * 60) + 5, 50);
  graph.add_edge(b, c, "B-C", 9 * 60, 60);
  graph.add_edge(a, c, "A-C-slow", (8 * 60) + 10, 500);

  const auto start = iso_to_date("2026-06-08 08:00:00");
  const auto path =
    graph.solver.find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
      a, c, start);
  expect_not_empty(path, "forward path exists");
  expect_eq(node_codes(path), std::vector<std::string>({"A", "B", "C"}),
            "forward chooses faster two-hop path");
  expect_eq(edge_codes(path), std::vector<std::string>({"A-B", "B-C"}),
            "forward edge sequence");
  expect_eq(minutes_between(last_step(path).distance, start), 120,
            "forward arrival includes wait between hops");

  GraphBuilder direct_graph;
  const auto da = direct_graph.add_center("A");
  const auto db = direct_graph.add_center("B");
  const auto dc = direct_graph.add_center("C");
  direct_graph.add_edge(da, db, "A-B-slow", (8 * 60) + 5, 200);
  direct_graph.add_edge(db, dc, "B-C-slow", 12 * 60, 100);
  direct_graph.add_edge(da, dc, "A-C-fast", (8 * 60) + 10, 30);
  const auto direct_path =
    direct_graph.solver
      .find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
        da, dc, start);
  expect_not_empty(direct_path, "direct forward path exists");
  expect_eq(node_codes(direct_path), std::vector<std::string>({"A", "C"}),
            "forward chooses direct path when faster");

  GraphBuilder unreachable_graph;
  const auto ua = unreachable_graph.add_center("A");
  const auto uc = unreachable_graph.add_center("C");
  const auto missing_path =
    unreachable_graph.solver
      .find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
        ua, uc, start);
  expect_eq(missing_path.empty(), true, "unreachable forward target");

  GraphBuilder wait_graph;
  const auto wa = wait_graph.add_center("A");
  const auto wb = wait_graph.add_center("B");
  wait_graph.add_edge(wa, wb, "daily", 9 * 60, 30);
  const auto after_departure = iso_to_date("2026-06-08 10:00:00");
  const auto wait_path =
    wait_graph.solver
      .find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
        wa, wb, after_departure);
  expect_not_empty(wait_path, "daily wait path exists");
  expect_eq(minutes_between(last_step(wait_path).distance,
                            after_departure),
            (23 * 60) + 30, "forward rolls daily route after departure");
}

void test_reverse_path_selection() {
  GraphBuilder graph;
  const auto a = graph.add_center("A");
  const auto b = graph.add_center("B");
  graph.add_edge(a, b, "A-B", 9 * 60, 60);

  const auto deadline = iso_to_date("2026-06-08 12:00:00");
  const auto path =
    graph.solver.find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
      b, a, deadline);
  expect_not_empty(path, "reverse path exists");
  expect_eq(node_codes(path), std::vector<std::string>({"A", "B"}),
            "reverse path node order");
  expect_eq(edge_codes(path), std::vector<std::string>({"A-B"}),
            "reverse edge sequence");
  expect_eq(minutes_between(deadline, path.front().distance), 180,
            "reverse latest source departure");

  GraphBuilder choice_graph;
  const auto ca = choice_graph.add_center("A");
  const auto cb = choice_graph.add_center("B");
  const auto cc = choice_graph.add_center("C");
  choice_graph.add_edge(ca, cc, "A-C-early", 9 * 60, 120);
  choice_graph.add_edge(ca, cb, "A-B-late", 10 * 60, 30);
  choice_graph.add_edge(cb, cc, "B-C-late", 11 * 60, 30);
  const auto choice_path =
    choice_graph.solver
      .find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
        cc, ca, deadline);
  expect_not_empty(choice_path, "reverse choice path exists");
  expect_eq(node_codes(choice_path), std::vector<std::string>({"A", "B", "C"}),
            "reverse chooses latest feasible two-hop path");
  expect_eq(edge_codes(choice_path),
            std::vector<std::string>({"A-B-late", "B-C-late"}),
            "reverse chosen edge sequence");
  expect_eq(minutes_between(deadline, choice_path.front().distance), 120,
            "reverse chosen path latest departure");

  GraphBuilder unreachable_graph;
  const auto ua = unreachable_graph.add_center("A");
  const auto ub = unreachable_graph.add_center("B");
  const auto missing_path =
    unreachable_graph.solver
      .find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
        ub, ua, deadline);
  expect_eq(missing_path.empty(), true, "unreachable reverse target");
}

void test_days_of_week_graph_behavior() {
  const auto monday_start = iso_to_date("2026-06-08 08:00:00");
  const auto monday_after = iso_to_date("2026-06-08 10:00:00");
  const auto sunday_start = iso_to_date("2026-06-07 08:00:00");

  GraphBuilder monday_graph;
  const auto ma = monday_graph.add_center("A");
  const auto mb = monday_graph.add_center("B");
  monday_graph.add_edge(ma, mb, "monday", 9 * 60, 30, day_mask(1));
  const auto monday_path =
    monday_graph.solver
      .find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
        ma, mb, monday_start);
  expect_not_empty(monday_path, "Monday route available same day");
  expect_eq(minutes_between(last_step(monday_path).distance, monday_start),
            90, "Monday same-day route");
  const auto next_monday_path =
    monday_graph.solver
      .find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
        ma, mb, monday_after);
  expect_not_empty(next_monday_path, "Monday route available next week");
  expect_eq(minutes_between(last_step(next_monday_path).distance,
                            monday_after),
            (6 * 24 * 60) + (23 * 60) + 30,
            "Monday-only route rolls to next Monday");

  GraphBuilder skip_graph;
  const auto sa = skip_graph.add_center("A");
  const auto sb = skip_graph.add_center("B");
  skip_graph.add_edge(sa,
                      sb,
                      "mon-sat",
                      9 * 60,
                      15,
                      static_cast<std::uint8_t>(ALL_DAYS_OF_WEEK &
                                                ~day_mask(0)));
  const auto skip_path =
    skip_graph.solver
      .find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
        sa, sb, sunday_start);
  expect_not_empty(skip_path, "Mon-Sat route skips Sunday");
  expect_eq(minutes_between(last_step(skip_path).distance, sunday_start),
            (25 * 60) + 15, "forward skips Sunday to Monday");
  const auto reverse_skip =
    skip_graph.solver
      .find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
        sb, sa, sunday_start);
  expect_not_empty(reverse_skip, "reverse Mon-Sat route skips Sunday");
  expect_eq(minutes_between(sunday_start, reverse_skip.front().distance),
            23 * 60, "reverse uses previous Saturday");

  GraphBuilder competing_graph;
  const auto ca = competing_graph.add_center("A");
  const auto cc = competing_graph.add_center("C");
  competing_graph.add_edge(ca, cc, "fast-next-week", 9 * 60, 30, day_mask(1));
  competing_graph.add_edge(cc, cc, "unused", 9 * 60, 1);
  competing_graph.add_edge(ca, cc, "slow-today", 10 * 60, 60, day_mask(2));
  const auto tuesday_start = iso_to_date("2026-06-09 08:00:00");
  const auto competing_path =
    competing_graph.solver
      .find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
        ca, cc, tuesday_start);
  expect_not_empty(competing_path, "competing day path exists");
  expect_eq(edge_codes(competing_path), std::vector<std::string>({"slow-today"}),
            "available slower route beats unavailable fast route");

  GraphBuilder multi_day_graph;
  const auto ga = multi_day_graph.add_center("A");
  const auto gb = multi_day_graph.add_center("B");
  const auto gc = multi_day_graph.add_center("C");
  multi_day_graph.add_edge(ga, gb, "A-B-mon", 9 * 60, 30, day_mask(1));
  multi_day_graph.add_edge(gb, gc, "B-C-tue", 9 * 60, 30, day_mask(2));
  const auto multi_day_path =
    multi_day_graph.solver
      .find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
        ga, gc, monday_start);
  expect_not_empty(multi_day_path, "multi-day path exists");
  expect_eq(minutes_between(last_step(multi_day_path).distance,
                            monday_start),
            (25 * 60) + 30, "multi-hop path waits for next day second leg");

  GraphBuilder missed_leg_graph;
  const auto la = missed_leg_graph.add_center("A");
  const auto lb = missed_leg_graph.add_center("B");
  const auto lc = missed_leg_graph.add_center("C");
  missed_leg_graph.add_edge(la, lb, "A-B", 8 * 60, 61, day_mask(1));
  missed_leg_graph.add_edge(lb, lc, "B-C", 9 * 60, 30, day_mask(1));
  const auto missed_path =
    missed_leg_graph.solver
      .find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
        la, lc, monday_start);
  expect_not_empty(missed_path, "missed second leg path exists");
  expect_eq(minutes_between(last_step(missed_path).distance, monday_start),
            (7 * 24 * 60) + 90,
            "second leg rolls to following Monday after one-minute miss");
}

void test_vehicle_filtering() {
  const auto start = iso_to_date("2026-06-08 08:00:00");
  GraphBuilder graph;
  const auto a = graph.add_center("A");
  const auto b = graph.add_center("B");
  graph.add_edge(a, b, "air", 9 * 60, 30, ALL_DAYS_OF_WEEK, VehicleType::AIR);

  const auto surface_path =
    graph.solver.find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
      a, b, start);
  expect_eq(surface_path.empty(), true, "surface filter excludes air edge");
  const auto air_path =
    graph.solver.find_path<PathTraversalMode::FORWARD, VehicleType::AIR>(
      a, b, start);
  expect_not_empty(air_path, "air filter includes air edge");

  GraphBuilder choice_graph;
  const auto ca = choice_graph.add_center("A");
  const auto cb = choice_graph.add_center("B");
  choice_graph.add_edge(ca, cb, "surface-slow", 9 * 60, 120,
                        ALL_DAYS_OF_WEEK, VehicleType::SURFACE);
  choice_graph.add_edge(ca, cb, "air-fast", 9 * 60, 30, ALL_DAYS_OF_WEEK,
                        VehicleType::AIR);
  const auto surface_choice =
    choice_graph.solver
      .find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
        ca, cb, start);
  expect_not_empty(surface_choice, "surface choice path exists");
  expect_eq(edge_codes(surface_choice),
            std::vector<std::string>({"surface-slow"}),
            "surface query chooses surface edge only");
  const auto air_choice =
    choice_graph.solver.find_path<PathTraversalMode::FORWARD, VehicleType::AIR>(
      ca, cb, start);
  expect_not_empty(air_choice, "air choice path exists");
  expect_eq(edge_codes(air_choice), std::vector<std::string>({"air-fast"}),
            "air query can choose air edge");
}

void test_route_edge_spec_expansion() {
  const auto route = make_base_route();
  const auto specs = build_route_edge_specs(route, IST_OFFSET);
  expect_eq(specs.size(), std::size_t{3}, "loading stops create N choose 2 edges");
  expect_eq(specs[0].source_center_code, std::string{"A"}, "first source");
  expect_eq(specs[0].target_center_code, std::string{"C"}, "first target");
  expect_eq(specs[1].source_center_code, std::string{"A"}, "second source");
  expect_eq(specs[1].target_center_code, std::string{"D"}, "second target");
  expect_eq(specs[2].source_center_code, std::string{"C"}, "third source");
  expect_eq(specs[2].target_center_code, std::string{"D"}, "third target");
  expect_eq(specs[0].edge.code, std::string{"route.0"}, "first edge code");
  expect_eq(specs[2].edge.code, std::string{"route.2"}, "third edge code");
  expect_eq(specs[0].edge.vehicle, VehicleType::AIR,
            "air route maps to air vehicle");
  expect_eq(specs[0].edge.movement, MovementType::LINEHAUL,
            "air route maps to linehaul movement");
  expect_eq(specs[0].edge.terminal, false, "non-final target not terminal");
  expect_eq(specs[1].edge.terminal, true, "final target terminal");
  expect_eq(specs[0].edge.days_of_week,
            static_cast<std::uint8_t>(ALL_DAYS_OF_WEEK & ~day_mask(0)),
            "days copied to route edges");
  expect_eq(specs[0].edge.duration_loading.count(), 30,
            "source halt window is halved for loading");
  expect_eq(specs[0].edge.duration_unloading.count(), 15,
            "target halt window is halved for unloading");
  expect_eq(specs[1].edge.duration_unloading.count(), 23,
            "odd target halt window half rounds up");

  {
    auto j = make_base_route_json();
    j["route_type"] = "carting";
    const auto carting_route = TestDoc::from(j);
    const auto carting_specs = build_route_edge_specs(carting_route, IST_OFFSET);
    expect_eq(carting_specs[0].edge.vehicle, VehicleType::SURFACE,
              "carting maps to surface vehicle");
    expect_eq(carting_specs[0].edge.movement, MovementType::CARTING,
              "carting maps to carting movement");
  }

  {
    auto j = make_base_route_json();
    j["route_type"] = "rail";
    const auto unknown_route = TestDoc::from(j);
    const auto unknown_specs = build_route_edge_specs(unknown_route, IST_OFFSET);
    expect_eq(unknown_specs[0].edge.vehicle, VehicleType::SURFACE,
              "unknown route type maps to surface vehicle");
    expect_eq(unknown_specs[0].edge.movement, MovementType::LINEHAUL,
              "unknown route type maps to linehaul movement");
  }

  {
    auto j = make_base_route_json();
    j["halt_centers"] = nlohmann::json::array({j["halt_centers"][0]});
    const auto one_stop_route = TestDoc::from(j);
    expect_eq(build_route_edge_specs(one_stop_route, IST_OFFSET).empty(), true,
              "one stop route produces no edges");
  }

  {
    auto j = make_base_route_json();
    for (auto& stop : j["halt_centers"]) {
      stop["loading_allowed"] = false;
    }
    const auto all_blocked_route = TestDoc::from(j);
    expect_eq(build_route_edge_specs(all_blocked_route, IST_OFFSET).empty(), true,
              "all non-loading route produces no edges");
  }

  {
    auto j = make_base_route_json();
    j["halt_centers"][1]["loading_allowed"] = "false";
    const auto non_boolean_loading_route = TestDoc::from(j);
    const auto non_boolean_specs =
      build_route_edge_specs(non_boolean_loading_route, IST_OFFSET);
    expect_eq(non_boolean_specs.size(), std::size_t{6},
              "non-boolean loading_allowed remains included");
  }

  {
    auto j = make_base_route_json();
    j.erase("route_schedule_uuid");
    const auto invalid_route = TestDoc::from(j);
    expect_eq(build_route_edge_specs(invalid_route, IST_OFFSET).empty(), true,
              "missing route fields return no specs");
  }

  {
    auto j = make_base_route_json();
    j["halt_centers"][2].erase("center_code");
    const auto invalid_stop_route = TestDoc::from(j);
    const auto invalid_stop_specs =
      build_route_edge_specs(invalid_stop_route, IST_OFFSET);
    expect_eq(invalid_stop_specs.size(), std::size_t{1},
              "invalid halt center skips affected edges only");
    expect_eq(invalid_stop_specs[0].source_center_code, std::string{"A"},
              "remaining valid source after invalid stop");
    expect_eq(invalid_stop_specs[0].target_center_code, std::string{"D"},
              "remaining valid target after invalid stop");
  }

  {
    auto j = make_base_route_json();
    j["halt_centers"][2]["rel_eta"] = "0:30";
    const auto negative_duration_route = TestDoc::from(j);
    const auto negative_specs =
      build_route_edge_specs(negative_duration_route, IST_OFFSET);
    expect_eq(negative_specs.size(), std::size_t{2},
              "negative duration skips affected edges");
    expect_eq(negative_specs[0].target_center_code, std::string{"D"},
              "negative duration keeps valid A-D edge");
    expect_eq(negative_specs[1].source_center_code, std::string{"C"},
              "negative duration keeps valid C-D edge");
  }

  {
    auto j = make_base_route_json();
    j["days_of_week"] = nlohmann::json::array({"bad", true});
    const auto default_days_route = TestDoc::from(j);
    const auto default_days_specs =
      build_route_edge_specs(default_days_route, IST_OFFSET);
    expect_eq(default_days_specs[0].edge.days_of_week, ALL_DAYS_OF_WEEK,
              "invalid days default to all days");
  }

  {
    auto j = make_base_route_json();
    j["reporting_time"] = "01:00";
    j["halt_centers"][0]["rel_etd"] = "0:30";
    const auto negative_offset_route = TestDoc::from(j);
    const auto negative_offset_specs =
      build_route_edge_specs(negative_offset_route, IST_OFFSET);
    expect_eq(negative_offset_specs[0].edge.departure.count(), -240,
              "negative reporting offset is preserved");
  }
}

void test_large_route_edge_spec_expansion() {
  nlohmann::json stops = nlohmann::json::array();
  for (int index = 0; index < 12; ++index) {
    stops.push_back({
      {"center_code", std::format("N{}", index)},
      {"rel_eta", std::format("{}:00", index * 2)},
      {"rel_etd", std::format("{}:30", index * 2)},
      {"loading_allowed", index % 3 != 0},
    });
  }

  nlohmann::json route_json = {
    {"route_schedule_uuid", "large-route"},
    {"name", "Large Route"},
    {"route_type", "carting"},
    {"reporting_time", "06:00"},
    {"days_of_week", {1, 2, 3, 4, 5, 6}},
    {"halt_centers", stops},
  };

  const auto route = TestDoc::from(route_json);
  const auto started = std::chrono::steady_clock::now();
  const auto specs = build_route_edge_specs(route, IST_OFFSET);
  const auto elapsed = std::chrono::steady_clock::now() - started;

  expect_eq(specs.size(), std::size_t{28},
            "large route expands N choose 2 after loading filter");
  expect_eq(specs.front().source_center_code, std::string{"N1"},
            "first included source after filtering");
  expect_eq(specs.back().target_center_code, std::string{"N11"},
            "last included target after filtering");
  expect_true(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                  .count() < 1000,
              "large route expansion smoke test stays bounded");
}

void test_real_route_fixture_edge_expansion() {
  const auto fixture = load_json_fixture("real_routes.json");
  const auto* routes = moirai::find_array_member(fixture, "data");
  expect_true(routes != nullptr && moirai::json_size(*routes) > 0,
              "real route fixture has routes");

  bool saw_blocked_stop = false;
  bool saw_restricted_days = false;
  bool saw_day_prefixed_time = false;
  bool saw_air = false;
  bool saw_carting = false;

  for (const auto& route : *routes) {
    const auto specs = build_route_edge_specs(route, IST_OFFSET);
    const auto loading_count = loading_stop_count(route);
    const auto expected_edges = (loading_count * (loading_count - 1U)) / 2U;
    expect_eq(specs.size(), expected_edges,
              "real route expands N choose 2 after loading filter");

    const auto days = parse_route_days_of_week(route);
    const auto* days_json = moirai::find_array_member(route, "days_of_week");
    saw_restricted_days =
      saw_restricted_days || (days_json != nullptr && moirai::json_size(*days_json) < 7);
    saw_blocked_stop = saw_blocked_stop || has_blocked_stop(route);
    saw_day_prefixed_time =
      saw_day_prefixed_time || has_day_prefixed_time(route);

    const auto route_type = moirai::find_string_member(route, "route_type");
    if (route_type.has_value()) {
      const auto lowered = lower_copy(*route_type);
      saw_air = saw_air || lowered == "air";
      saw_carting = saw_carting || lowered == "carting";
    }

    const auto blocked_codes = blocked_center_codes(route);
    for (const auto& spec : specs) {
      expect_eq(spec.edge.days_of_week, days,
                "real route edge preserves day mask");
      expect_true(!std::ranges::contains(blocked_codes,
                                         spec.source_center_code),
                  "blocked stop is not an edge source");
      expect_true(!std::ranges::contains(blocked_codes,
                                         spec.target_center_code),
                  "blocked stop is not an edge target");
    }
  }

  expect_true(saw_blocked_stop, "real fixture includes loading_allowed false");
  expect_true(saw_restricted_days, "real fixture includes restricted days");
  expect_true(saw_day_prefixed_time, "real fixture includes day-prefixed time");
  expect_true(saw_air, "real fixture includes air route");
  expect_true(saw_carting, "real fixture includes carting route");
}

void test_real_route_fixture_scheduled_paths() {
  const auto fixture = load_json_fixture("real_routes.json");
  const auto* routes = moirai::find_array_member(fixture, "data");
  expect_true(routes != nullptr && moirai::json_size(*routes) > 0,
              "real route fixture has routes for path test");

  std::optional<moirai::Json> restricted_route;
  for (const auto& route : *routes) {
    const auto* days = moirai::find_array_member(route, "days_of_week");
    if (days != nullptr && moirai::json_size(*days) < 7) {
      restricted_route = route;
      break;
    }
  }
  expect_true(restricted_route.has_value(),
              "real fixture has restricted-day route");

  const auto specs = build_route_edge_specs(*restricted_route, IST_OFFSET);
  expect_true(!specs.empty(), "restricted real route produces an edge");
  const auto& edge_spec = specs.front();

  GraphBuilder graph;
  const auto source = graph.add_center(edge_spec.source_center_code);
  const auto target = graph.add_center(edge_spec.target_center_code);
  auto expected_edge = edge_spec.edge;
  expected_edge.update(*graph.solver.get_node(source),
                       *graph.solver.get_node(target));
  expect_true(graph.solver.add_edge(source, target, edge_spec.edge) !=
                  INVALID_EDGE,
              "real route edge inserted");

  const auto monday_start = iso_to_date("2026-06-08 00:00:00");
  CalcualateTraversalCost calculator;
  const auto expected_arrival =
    calculator.operator()<PathTraversalMode::FORWARD>(
      monday_start, expected_edge.weight<PathTraversalMode::FORWARD>());
  const auto forward_path =
    graph.solver.find_path<PathTraversalMode::FORWARD, VehicleType::AIR>(
      source, target, monday_start);
  expect_not_empty(forward_path, "real restricted forward path exists");
  expect_eq(last_step(forward_path).distance, expected_arrival,
            "real restricted forward path follows day mask");
  expect_true(minutes_between(expected_arrival, monday_start) > 24 * 60,
              "real restricted forward path skips disallowed Monday");

  const auto monday_deadline = iso_to_date("2026-06-08 23:00:00");
  const auto expected_departure =
    calculator.operator()<PathTraversalMode::REVERSE>(
      monday_deadline, expected_edge.weight<PathTraversalMode::REVERSE>()) +
    expected_edge.source_offset();
  const auto reverse_path =
    graph.solver.find_path<PathTraversalMode::REVERSE, VehicleType::AIR>(
      target, source, monday_deadline);
  expect_not_empty(reverse_path, "real restricted reverse path exists");
  expect_eq(reverse_path.front().distance, expected_departure,
            "real restricted reverse path follows day mask");
  expect_true(minutes_between(monday_deadline, expected_departure) > 24 * 60,
              "real restricted reverse path skips disallowed Monday");
}

void test_capacity_owner_lifecycle() {
  CapacityReservationStore store;
  store.set_enabled(true);
  const CapacityInstanceKey key{.edge = 7, .departure = 100};
  store.set_capacity(key, 2);

  const std::array instances{key};
  const auto first =
    store.reserve_for("shipment-1", std::span<const CapacityInstanceKey>{instances}, 1);
  expect_true(first.has_value(), "owner reservation succeeds");
  expect_eq(store.remaining(key).value_or(99), CapacityUnits{1},
            "reservation reduces remaining capacity");

  const auto second =
    store.reserve_for("shipment-1", std::span<const CapacityInstanceKey>{instances}, 1);
  expect_true(second.has_value(), "same owner reservation is idempotent");
  expect_eq(store.remaining(key).value_or(99), CapacityUnits{1},
            "same owner does not double reserve");

  const auto consumed = store.consume("shipment-1");
  expect_true(consumed.has_value(), "owner reservation can be consumed");
  expect_eq(store.remaining(key).value_or(99), CapacityUnits{1},
            "consumed capacity remains unavailable");
  expect_true(!store.reservation("shipment-1").has_value(),
              "consume clears owner reservation");
}

void test_capacity_bucket_invariants_and_delta_records() {
  CapacityReservationStore store;
  store.set_enabled(true);
  const CapacityInstanceKey key{.edge = 8, .departure = 200};

  store.set_bucket(key,
                   CapacityBucket{
                     .capacity = 5,
                     .reserved = 6,
                     .consumed = 2,
                     .protected_units = 9,
                     .price = -10.0,
                   });
  const auto clamped = store.bucket(key);
  expect_true(clamped.has_value(), "clamped bucket exists");
  expect_eq(clamped->capacity, CapacityUnits{5}, "capacity preserved");
  expect_eq(clamped->reserved, CapacityUnits{3},
            "reserved is clamped to capacity minus consumed");
  expect_eq(clamped->consumed, CapacityUnits{2}, "consumed preserved");
  expect_eq(clamped->protected_units, CapacityUnits{5},
            "protected units are clamped to capacity");
  expect_eq(clamped->price, 0.0, "negative price is clamped");
  expect_eq(store.metrics().invalid_buckets_clamped, std::uint64_t{1},
            "invalid bucket clamp is counted");

  moirai::apply_capacity_ingestion_record(
    store,
    moirai::CapacityIngestionRecord{
      .key = key,
      .mode = moirai::CapacityUpdateMode::SNAPSHOT,
      .capacity = CapacityUnits{10},
      .reserved = CapacityUnits{1},
      .consumed = CapacityUnits{2},
      .protected_units = CapacityUnits{3},
      .price = 4.0,
      .reserve_ratio = std::nullopt,
      .min_protected_units = std::nullopt,
      .max_protected_units = std::nullopt,
    });
  moirai::apply_capacity_ingestion_record(
    store,
    moirai::CapacityIngestionRecord{
      .key = key,
      .mode = moirai::CapacityUpdateMode::DELTA,
      .capacity = CapacityUnits{5},
      .reserved = CapacityUnits{2},
      .consumed = CapacityUnits{1},
      .protected_units = CapacityUnits{4},
      .price = 1.5,
      .reserve_ratio = std::nullopt,
      .min_protected_units = std::nullopt,
      .max_protected_units = std::nullopt,
    });
  const auto delta = store.bucket(key);
  expect_true(delta.has_value(), "delta bucket exists");
  expect_eq(delta->capacity, CapacityUnits{15}, "delta adds capacity");
  expect_eq(delta->reserved, CapacityUnits{3}, "delta adds reserved units");
  expect_eq(delta->consumed, CapacityUnits{3}, "delta adds consumed units");
  expect_eq(delta->protected_units, CapacityUnits{7},
            "delta adds protected units");
  expect_eq(delta->price, 5.5, "delta adds price");
}

void test_capacity_reservation_metadata_expiry_and_freeze() {
  CapacityReservationStore store;
  store.set_enabled(true);
  const CapacityInstanceKey first{.edge = 9, .departure = 300};
  const CapacityInstanceKey second{.edge = 10, .departure = 300};
  store.set_capacity(first, 2);
  store.set_capacity(second, 2);

  const std::array first_instances{first};
  const auto frozen = store.reserve_for(
    "shipment-freeze",
    std::span<const CapacityInstanceKey>{first_instances},
    1,
    CapacityReservationOptions{
      .reserved_at = CLOCK{CLOCK::duration{10}},
      .expires_at = CLOCK{CLOCK::duration{50}},
      .frozen_at = CLOCK{CLOCK::duration{40}},
      .payload = R"({"id":"shipment-freeze"})",
    });
  expect_true(frozen.has_value(), "metadata reservation succeeds");
  expect_eq(frozen->payload, std::string{R"({"id":"shipment-freeze"})"},
            "payload is preserved");
  expect_true(store.expire_before(CLOCK{CLOCK::duration{49}}).empty(),
              "reservation before expiry is retained");

  const std::array second_instances{second};
  const auto frozen_replace = store.reserve_for(
    "shipment-freeze",
    std::span<const CapacityInstanceKey>{second_instances},
    1,
    CapacityReservationOptions{
      .reserved_at = CLOCK{CLOCK::duration{40}},
      .expires_at = CLOCK{CLOCK::duration{80}},
      .frozen_at = CLOCK{CLOCK::duration{70}},
      .payload = "replacement",
    });
  expect_true(!frozen_replace.has_value(),
              "different reservation cannot replace a frozen owner");
  const auto frozen_same = store.reserve_for(
    "shipment-freeze",
    std::span<const CapacityInstanceKey>{first_instances},
    1,
    CapacityReservationOptions{
      .reserved_at = CLOCK{CLOCK::duration{40}},
      .expires_at = CLOCK{CLOCK::duration{80}},
      .frozen_at = CLOCK{CLOCK::duration{70}},
      .payload = "same",
    });
  expect_true(frozen_same.has_value(),
              "same reservation remains idempotent after freeze");

  const auto expired = store.expire_before(CLOCK{CLOCK::duration{50}});
  expect_eq(expired.size(), std::size_t{1}, "expired reservation returned");
  expect_true(!store.reservation("shipment-freeze").has_value(),
              "expired reservation is removed");
  expect_eq(store.metrics().reservations_expired, std::uint64_t{1},
            "expiry metric is counted");
}

void test_capacity_protection_dual_prices_and_candidates() {
  GraphBuilder graph;
  const auto a = graph.add_center("A");
  const auto b = graph.add_center("B");
  const auto c = graph.add_center("C");
  graph.add_edge(a, b, "A-B", 9 * 60, 30);
  graph.add_edge(a, c, "A-C", 9 * 60, 30);
  graph.add_edge(c, b, "C-B", 10 * 60, 30);

  const auto direct = graph.solver.find_edge("A-B");
  const auto first_leg = graph.solver.find_edge("A-C");
  const auto second_leg = graph.solver.find_edge("C-B");
  expect_true(direct.has_value() && first_leg.has_value() && second_leg.has_value(),
              "test edges are indexed");

  CapacityReservationStore store;
  store.set_enabled(true);
  store.set_missing_capacity_allowed(false);
  const auto direct_departure =
    static_cast<SolverMinute>(iso_to_date("2026-06-08 09:00:00")
                                .time_since_epoch()
                                .count());
  const auto second_departure =
    static_cast<SolverMinute>(iso_to_date("2026-06-08 10:00:00")
                                .time_since_epoch()
                                .count());
  store.set_capacity({.edge = *direct, .departure = direct_departure}, 10);
  store.set_capacity({.edge = *first_leg, .departure = direct_departure}, 10);
  store.set_capacity({.edge = *second_leg, .departure = second_departure}, 10);

  CapacitySearch search{
    .store = &store,
    .units = 1,
    .search_weeks = 1,
  };
  PathCandidateMetrics metrics;
  const auto candidates =
    graph.solver.find_path_candidates<PathTraversalMode::FORWARD,
                                      VehicleType::SURFACE>(
      a,
      b,
      iso_to_date("2026-06-08 08:00:00"),
      search,
      PathCandidateOptions{
        .max_candidates = 2,
        .search_weeks = 1,
        .max_expansions = 16,
        .deadline = iso_to_date("2026-06-08 12:00:00"),
        .metrics = &metrics,
      });
  expect_eq(candidates.size(), std::size_t{2},
            "candidate generation returns direct and alternate paths");
  expect_eq(metrics.generated, std::size_t{2},
            "candidate metrics record generated paths");
  expect_true(metrics.expansions > 0,
              "candidate metrics record bounded expansions");
  expect_eq(metrics.slack_duplicates, std::size_t{0},
            "candidate metrics expose slack diversity");
  expect_eq(edge_codes(candidates.front()), std::vector<std::string>{"A-B"},
            "first candidate is direct path");

  store.set_protected_capacity({.edge = *direct, .departure = direct_departure}, 10);
  const auto protected_path =
    graph.solver.find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
      a,
      b,
      iso_to_date("2026-06-08 08:00:00"),
      search);
  expect_eq(edge_codes(protected_path),
            std::vector<std::string>({"A-C", "C-B"}),
            "protected capacity is unavailable to normal search");

  store.set_protected_capacity({.edge = *direct, .departure = direct_departure}, 0);
  store.set_price({.edge = *direct, .departure = direct_departure}, 10'000.0);
  CapacitySearch priced_search = search;
  priced_search.price_weight = 1.0;
  const auto priced_path =
    graph.solver.find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
      a,
      b,
      iso_to_date("2026-06-08 08:00:00"),
      priced_search);
  expect_eq(edge_codes(priced_path),
            std::vector<std::string>({"A-C", "C-B"}),
	            "dual price steers path away from scarce direct edge");
}

void test_capacity_forecast_protection_policy() {
  CapacityReservationStore store;
  store.set_enabled(true);
  const CapacityInstanceKey first{.edge = 11, .departure = 100};
  const CapacityInstanceKey second{.edge = 12, .departure = 100};
  const CapacityInstanceKey third{.edge = 13, .departure = 100};

  const auto forecast_doc = TestDoc::from(nlohmann::json{
    {"edge", first.edge},
    {"departure", first.departure},
    {"capacity", 10},
    {"forecast_demand", 4},
  });
  const auto forecast_record =
    moirai::parse_capacity_ingestion_record(forecast_doc.element);
  expect_true(forecast_record.has_value(), "forecast capacity record parses");
  moirai::apply_capacity_ingestion_record(store, *forecast_record);
  expect_eq(store.protected_capacity(first), CapacityUnits{4},
            "forecast demand seeds protected capacity");

  moirai::apply_capacity_ingestion_record(
    store,
    moirai::CapacityIngestionRecord{
      .key = second,
      .capacity = CapacityUnits{10},
      .reserved = std::nullopt,
      .consumed = std::nullopt,
      .protected_units = std::nullopt,
      .price = std::nullopt,
      .reserve_ratio = std::nullopt,
      .min_protected_units = std::nullopt,
      .max_protected_units = std::nullopt,
    },
    moirai::CapacityProtectionPolicy{
      .reserve_ratio = 0.25,
      .min_protected_units = 3,
      .max_protected_units = CapacityUnits{4},
    });
  expect_eq(store.protected_capacity(second), CapacityUnits{3},
            "ratio policy derives protected capacity");

  moirai::apply_capacity_ingestion_record(
    store,
    moirai::CapacityIngestionRecord{
      .key = second,
      .capacity = CapacityUnits{10},
      .reserved = std::nullopt,
      .consumed = std::nullopt,
      .protected_units = CapacityUnits{1},
      .price = std::nullopt,
      .reserve_ratio = std::nullopt,
      .min_protected_units = std::nullopt,
      .max_protected_units = std::nullopt,
    },
    moirai::CapacityProtectionPolicy{
      .reserve_ratio = 0.75,
      .min_protected_units = 3,
      .max_protected_units = std::nullopt,
    });
  expect_eq(store.protected_capacity(second), CapacityUnits{1},
            "explicit protected capacity overrides forecast policy");

  moirai::apply_capacity_ingestion_record(
    store,
    moirai::CapacityIngestionRecord{
      .key = third,
      .mode = moirai::CapacityUpdateMode::SNAPSHOT,
      .capacity = CapacityUnits{10},
      .reserved = std::nullopt,
      .consumed = std::nullopt,
      .protected_units = std::nullopt,
      .price = std::nullopt,
      .reserve_ratio = 0.40,
      .min_protected_units = CapacityUnits{5},
      .max_protected_units = CapacityUnits{6},
    },
    moirai::CapacityProtectionPolicy{});
  expect_eq(store.protected_capacity(third), CapacityUnits{5},
            "record-level protection policy overrides defaults");
  expect_eq(store.available(third, CapacityUnits{6}, false),
            false,
            "normal availability respects protected capacity");
  expect_eq(store.available(third, CapacityUnits{6}, true),
            true,
            "explicit protected override can use protected capacity");
  expect_eq(store.metrics().protected_capacity_saved, std::uint64_t{1},
            "protected-capacity deferral is counted");
  expect_eq(store.metrics().protected_capacity_used, std::uint64_t{1},
            "protected-capacity override use is counted");
}

void test_capacity_state_log_and_optimizer() {
  CapacityReservationStore store;
  store.set_enabled(true);
  const CapacityInstanceKey direct{.edge = 1, .departure = 100};
  const CapacityInstanceKey alternate{.edge = 2, .departure = 100};
  store.set_bucket(direct,
                   CapacityBucket{
                     .capacity = 2,
                     .reserved = 1,
                     .consumed = 0,
                     .protected_units = 0,
                     .price = 0.0,
                   });
  store.set_capacity(alternate, 2);
  const std::array direct_instances{direct};
  expect_true(store.reserve_for("existing",
                                std::span<const CapacityInstanceKey>{direct_instances},
                                1,
                                CapacityReservationOptions{
                                  .reserved_at = CLOCK{CLOCK::duration{1}},
                                  .expires_at = CLOCK{CLOCK::duration{500}},
                                  .frozen_at = CLOCK{CLOCK::duration{400}},
                                  .payload = "existing-payload",
                                })
                .has_value(),
              "existing reservation succeeds");

  const auto state_path =
    std::filesystem::temp_directory_path() /
    std::format("moirai-capacity-state-{}.log",
                std::chrono::steady_clock::now().time_since_epoch().count());
  moirai::CapacityStateLog state{state_path};
  state.persist_snapshot(store);

  CapacityReservationStore restored;
  restored.set_enabled(true);
  state.restore(restored);
  expect_eq(restored.remaining(direct).value_or(99), CapacityUnits{0},
            "state restore preserves reservation pressure");
  expect_true(restored.reservation("existing").has_value(),
              "state restore preserves owner reservation");
  expect_eq(restored.reservation("existing")->payload,
            std::string{"existing-payload"},
            "state restore preserves owner payload");

  const moirai::OptimizationShipment shipment{
    .id = "shipment",
    .candidates = {
      moirai::OptimizationPathCandidate{
        .capacity_instances = {direct},
        .units = 1,
        .hops = 1,
        .arrival = CLOCK{CLOCK::duration{120}},
        .deadline = CLOCK{CLOCK::duration{200}},
        .cost = 0.0,
        .sla_feasible = true,
      },
      moirai::OptimizationPathCandidate{
        .capacity_instances = {alternate},
        .units = 1,
        .hops = 2,
        .arrival = CLOCK{CLOCK::duration{150}},
        .deadline = CLOCK{CLOCK::duration{200}},
        .cost = 0.0,
        .sla_feasible = true,
      },
    },
    .units = 1,
    .deadline = CLOCK{CLOCK::duration{200}},
    .reservation_options = {},
    .priority = 1,
  };
  moirai::RollingHorizonConfig optimizer_config;
  optimizer_config.enabled = true;
  optimizer_config.use_highs = true;
  moirai::RollingHorizonOptimizer optimizer{optimizer_config};
  const std::array shipments{shipment};
  const auto result =
    optimizer.optimize(std::span<const moirai::OptimizationShipment>{shipments},
                       restored);
  expect_eq(result.chosen_candidate.size(), std::size_t{1},
            "optimizer returns one decision");
  expect_true(result.chosen_candidate.front().has_value(),
              "optimizer chooses a feasible candidate");
  expect_eq(*result.chosen_candidate.front(), std::size_t{1},
            "optimizer avoids exhausted direct capacity");
  expect_eq(optimizer.commit(
              std::span<const moirai::OptimizationShipment>{shipments},
              result,
              restored),
            std::size_t{1},
            "optimizer can commit chosen reservations");
  std::filesystem::remove(state_path);

#ifdef MOIRAI_WITH_ROCKSDB
  const auto rocksdb_path =
    std::filesystem::temp_directory_path() /
    std::format("moirai-capacity-rocksdb-{}",
                std::chrono::steady_clock::now().time_since_epoch().count());
  moirai::CapacityStateLog rocksdb_state{
    rocksdb_path,
    moirai::CapacityStateBackend::ROCKSDB,
  };
  rocksdb_state.persist_snapshot(restored);
  CapacityReservationStore rocksdb_restored;
  rocksdb_restored.set_enabled(true);
  rocksdb_state.restore(rocksdb_restored);
  expect_true(rocksdb_restored.reservation("existing").has_value(),
              "rocksdb restore preserves existing owner reservation");
  expect_true(rocksdb_restored.reservation("shipment").has_value(),
              "rocksdb restore preserves optimizer committed reservation");
  std::filesystem::remove_all(rocksdb_path);
#endif
}

void test_rolling_horizon_lexicographic_objective() {
  CapacityReservationStore store;
  store.set_enabled(true);
  const CapacityInstanceKey direct{.edge = 20, .departure = 100};
  const CapacityInstanceKey alternate_first{.edge = 21, .departure = 100};
  const CapacityInstanceKey alternate_second{.edge = 22, .departure = 110};
  store.set_capacity(direct, 1);
  store.set_capacity(alternate_first, 1);
  store.set_capacity(alternate_second, 1);

  const moirai::OptimizationShipment shipment{
    .id = "shipment-lex",
    .candidates = {
      moirai::OptimizationPathCandidate{
        .capacity_instances = {direct},
        .units = 1,
        .hops = 1,
        .arrival = CLOCK{CLOCK::duration{500}},
        .deadline = CLOCK{CLOCK::duration{200}},
        .cost = 0.0,
        .sla_feasible = false,
      },
      moirai::OptimizationPathCandidate{
        .capacity_instances = {alternate_first, alternate_second},
        .units = 1,
        .hops = 2,
        .arrival = CLOCK{CLOCK::duration{180}},
        .deadline = CLOCK{CLOCK::duration{200}},
        .cost = 0.0,
        .sla_feasible = true,
      },
    },
    .units = 1,
    .deadline = CLOCK{CLOCK::duration{200}},
    .reservation_options = {},
    .priority = 0,
  };
  moirai::RollingHorizonConfig config;
  config.use_highs = false;
  config.sla_breach_penalty = 1.0;
  config.hop_penalty = 1'000'000.0;
  config.lateness_penalty_per_minute = 1.0;
  moirai::RollingHorizonOptimizer optimizer{config};
  const std::array shipments{shipment};
  const auto result =
    optimizer.optimize(std::span<const moirai::OptimizationShipment>{shipments},
                       store);
  expect_true(result.chosen_candidate.front().has_value(),
              "lexicographic optimizer chooses a candidate");
  expect_eq(*result.chosen_candidate.front(), std::size_t{1},
            "SLA-feasible candidate dominates fewer-hop SLA breach");
}

} // namespace

auto main() -> int {
  test_graph_basics();
  test_value_overloads_and_csr_invalidation();
  test_concurrent_lazy_csr_rebuild_is_thread_safe();
  test_forward_path_selection();
  test_reverse_path_selection();
  test_days_of_week_graph_behavior();
  test_vehicle_filtering();
  test_route_edge_spec_expansion();
  test_large_route_edge_spec_expansion();
  test_real_route_fixture_edge_expansion();
  test_real_route_fixture_scheduled_paths();
  test_capacity_owner_lifecycle();
  test_capacity_bucket_invariants_and_delta_records();
  test_capacity_reservation_metadata_expiry_and_freeze();
  test_capacity_protection_dual_prices_and_candidates();
  test_capacity_forecast_protection_policy();
  test_capacity_state_log_and_optimizer();
  test_rolling_horizon_lexicographic_objective();
  return 0;
}
