#include "date_utils.hxx"
#include "route_schedule.hxx"
#include "solver.hxx"
#include "transportation.hxx"
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <cstdlib>
#include <format>
#include <iostream>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr auto IST_OFFSET = DURATION{330};

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
  std::exit(EXIT_FAILURE);
}

void expect_true(bool value, std::string_view label,
                 std::source_location location =
                   std::source_location::current()) {
  expect_eq(value, true, label, location);
}

void expect_not_null(const std::shared_ptr<Segment>& segment,
                     std::string_view label,
                     std::source_location location =
                       std::source_location::current()) {
  if (segment != nullptr) {
    return;
  }

  std::cerr << location.file_name() << ':' << location.line() << ": " << label
            << " failed\n";
  std::exit(EXIT_FAILURE);
}

struct GraphBuilder {
  Solver solver;

  auto add_center(std::string code) -> Node<Graph> {
    auto center = std::make_shared<TransportCenter>(std::move(code));
    const auto [node, inserted] = solver.add_node(center);
    expect_true(inserted, "center insertion");
    return node;
  }

  auto add_edge(Node<Graph> source, Node<Graph> target, std::string code,
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
    solver.add_edge(source, target, edge);
    return edge;
  }
};

auto node_codes(const std::shared_ptr<Segment>& segment)
    -> std::vector<std::string> {
  std::vector<std::string> codes;
  for (auto current = segment; current != nullptr; current = current->next) {
    codes.push_back(current->node->code);
  }
  return codes;
}

auto edge_codes(const std::shared_ptr<Segment>& segment)
    -> std::vector<std::string> {
  std::vector<std::string> codes;
  for (auto current = segment; current != nullptr && current->next != nullptr;
       current = current->next) {
    codes.push_back(current->outbound->code);
  }
  return codes;
}

auto last_segment(const std::shared_ptr<Segment>& segment)
    -> std::shared_ptr<Segment> {
  auto current = segment;
  while (current != nullptr && current->next != nullptr) {
    current = current->next;
  }
  return current;
}

auto make_base_route() -> moirai::Json {
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

void test_graph_basics() {
  GraphBuilder graph;
  const auto a = graph.add_center("A");
  const auto b = graph.add_center("B");

  const auto [existing_a, has_a] = graph.solver.add_node("A");
  expect_true(has_a, "existing node lookup succeeds");
  expect_eq(existing_a, a, "existing node descriptor is reused");

  const auto [missing, has_missing] = graph.solver.add_node("missing");
  expect_eq(has_missing, false, "missing node lookup fails");
  expect_eq(missing, Graph::null_vertex(), "missing node descriptor is null");

  graph.add_edge(a, b, "edge", 9 * 60, 60);
  const auto [existing_edge, has_edge] = graph.solver.add_edge("edge");
  expect_true(has_edge, "existing edge lookup succeeds");

  auto duplicate = std::make_shared<TransportEdge>(
    "edge", "duplicate", DURATION{600}, DURATION{60}, DURATION{0},
    DURATION{0}, VehicleType::SURFACE, MovementType::CARTING, false);
  const auto [duplicate_edge, duplicate_result] =
    graph.solver.add_edge(a, b, duplicate);
  expect_true(duplicate_result, "duplicate edge returns existing descriptor");
  expect_eq(duplicate_edge, existing_edge, "duplicate edge descriptor reused");
  expect_eq(graph.solver.show(), std::string{"Graph<2, 1>"},
            "duplicate edge does not change graph size");

  const auto graph_dump = graph.solver.show_all();
  expect_true(graph_dump.find("A") != std::string::npos,
              "show_all contains source node");
  expect_true(graph_dump.find("edge: A TO B") != std::string::npos,
              "show_all contains edge");
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
  expect_not_null(path, "forward path exists");
  expect_eq(node_codes(path), std::vector<std::string>({"A", "B", "C"}),
            "forward chooses faster two-hop path");
  expect_eq(edge_codes(path), std::vector<std::string>({"A-B", "B-C"}),
            "forward edge sequence");
  expect_eq(minutes_between(last_segment(path)->distance, start), 120,
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
  expect_not_null(direct_path, "direct forward path exists");
  expect_eq(node_codes(direct_path), std::vector<std::string>({"A", "C"}),
            "forward chooses direct path when faster");

  GraphBuilder unreachable_graph;
  const auto ua = unreachable_graph.add_center("A");
  const auto uc = unreachable_graph.add_center("C");
  const auto missing_path =
    unreachable_graph.solver
      .find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
        ua, uc, start);
  expect_eq(missing_path == nullptr, true, "unreachable forward target");

  GraphBuilder wait_graph;
  const auto wa = wait_graph.add_center("A");
  const auto wb = wait_graph.add_center("B");
  wait_graph.add_edge(wa, wb, "daily", 9 * 60, 30);
  const auto after_departure = iso_to_date("2026-06-08 10:00:00");
  const auto wait_path =
    wait_graph.solver
      .find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
        wa, wb, after_departure);
  expect_not_null(wait_path, "daily wait path exists");
  expect_eq(minutes_between(last_segment(wait_path)->distance,
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
  expect_not_null(path, "reverse path exists");
  expect_eq(node_codes(path), std::vector<std::string>({"A", "B"}),
            "reverse path node order");
  expect_eq(edge_codes(path), std::vector<std::string>({"A-B"}),
            "reverse edge sequence");
  expect_eq(minutes_between(deadline, path->distance), 180,
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
  expect_not_null(choice_path, "reverse choice path exists");
  expect_eq(node_codes(choice_path), std::vector<std::string>({"A", "B", "C"}),
            "reverse chooses latest feasible two-hop path");
  expect_eq(edge_codes(choice_path),
            std::vector<std::string>({"A-B-late", "B-C-late"}),
            "reverse chosen edge sequence");
  expect_eq(minutes_between(deadline, choice_path->distance), 120,
            "reverse chosen path latest departure");

  GraphBuilder unreachable_graph;
  const auto ua = unreachable_graph.add_center("A");
  const auto ub = unreachable_graph.add_center("B");
  const auto missing_path =
    unreachable_graph.solver
      .find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
        ub, ua, deadline);
  expect_eq(missing_path == nullptr, true, "unreachable reverse target");
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
  expect_not_null(monday_path, "Monday route available same day");
  expect_eq(minutes_between(last_segment(monday_path)->distance, monday_start),
            90, "Monday same-day route");
  const auto next_monday_path =
    monday_graph.solver
      .find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
        ma, mb, monday_after);
  expect_not_null(next_monday_path, "Monday route available next week");
  expect_eq(minutes_between(last_segment(next_monday_path)->distance,
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
  expect_not_null(skip_path, "Mon-Sat route skips Sunday");
  expect_eq(minutes_between(last_segment(skip_path)->distance, sunday_start),
            (25 * 60) + 15, "forward skips Sunday to Monday");
  const auto reverse_skip =
    skip_graph.solver
      .find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
        sb, sa, sunday_start);
  expect_not_null(reverse_skip, "reverse Mon-Sat route skips Sunday");
  expect_eq(minutes_between(sunday_start, reverse_skip->distance),
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
  expect_not_null(competing_path, "competing day path exists");
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
  expect_not_null(multi_day_path, "multi-day path exists");
  expect_eq(minutes_between(last_segment(multi_day_path)->distance,
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
  expect_not_null(missed_path, "missed second leg path exists");
  expect_eq(minutes_between(last_segment(missed_path)->distance, monday_start),
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
  expect_eq(surface_path == nullptr, true, "surface filter excludes air edge");
  const auto air_path =
    graph.solver.find_path<PathTraversalMode::FORWARD, VehicleType::AIR>(
      a, b, start);
  expect_not_null(air_path, "air filter includes air edge");

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
  expect_not_null(surface_choice, "surface choice path exists");
  expect_eq(edge_codes(surface_choice),
            std::vector<std::string>({"surface-slow"}),
            "surface query chooses surface edge only");
  const auto air_choice =
    choice_graph.solver.find_path<PathTraversalMode::FORWARD, VehicleType::AIR>(
      ca, cb, start);
  expect_not_null(air_choice, "air choice path exists");
  expect_eq(edge_codes(air_choice), std::vector<std::string>({"air-fast"}),
            "air query can choose air edge");
}

void test_route_edge_spec_expansion() {
  const auto route = make_base_route();
  const auto specs = build_route_edge_specs(route, IST_OFFSET);
  expect_eq(specs.size(), size_t{3}, "loading stops create N choose 2 edges");
  expect_eq(specs[0].source_center_code, std::string{"A"}, "first source");
  expect_eq(specs[0].target_center_code, std::string{"C"}, "first target");
  expect_eq(specs[1].source_center_code, std::string{"A"}, "second source");
  expect_eq(specs[1].target_center_code, std::string{"D"}, "second target");
  expect_eq(specs[2].source_center_code, std::string{"C"}, "third source");
  expect_eq(specs[2].target_center_code, std::string{"D"}, "third target");
  expect_eq(specs[0].edge->code, std::string{"route.0"}, "first edge code");
  expect_eq(specs[2].edge->code, std::string{"route.2"}, "third edge code");
  expect_eq(specs[0].edge->vehicle, VehicleType::AIR,
            "air route maps to air vehicle");
  expect_eq(specs[0].edge->movement, MovementType::LINEHAUL,
            "air route maps to linehaul movement");
  expect_eq(specs[0].edge->terminal, false, "non-final target not terminal");
  expect_eq(specs[1].edge->terminal, true, "final target terminal");
  expect_eq(specs[0].edge->days_of_week,
            static_cast<std::uint8_t>(ALL_DAYS_OF_WEEK & ~day_mask(0)),
            "days copied to route edges");

  auto carting_route = make_base_route();
  carting_route["route_type"] = "carting";
  const auto carting_specs = build_route_edge_specs(carting_route, IST_OFFSET);
  expect_eq(carting_specs[0].edge->vehicle, VehicleType::SURFACE,
            "carting maps to surface vehicle");
  expect_eq(carting_specs[0].edge->movement, MovementType::CARTING,
            "carting maps to carting movement");

  auto unknown_route = make_base_route();
  unknown_route["route_type"] = "rail";
  const auto unknown_specs = build_route_edge_specs(unknown_route, IST_OFFSET);
  expect_eq(unknown_specs[0].edge->vehicle, VehicleType::SURFACE,
            "unknown route type maps to surface vehicle");
  expect_eq(unknown_specs[0].edge->movement, MovementType::LINEHAUL,
            "unknown route type maps to linehaul movement");

  auto one_stop_route = make_base_route();
  one_stop_route["halt_centers"] = moirai::Json::array(
    {one_stop_route["halt_centers"][0]});
  expect_eq(build_route_edge_specs(one_stop_route, IST_OFFSET).empty(), true,
            "one stop route produces no edges");

  auto all_blocked_route = make_base_route();
  for (auto& stop : all_blocked_route["halt_centers"]) {
    stop["loading_allowed"] = false;
  }
  expect_eq(build_route_edge_specs(all_blocked_route, IST_OFFSET).empty(), true,
            "all non-loading route produces no edges");

  auto non_boolean_loading_route = make_base_route();
  non_boolean_loading_route["halt_centers"][1]["loading_allowed"] = "false";
  const auto non_boolean_specs =
    build_route_edge_specs(non_boolean_loading_route, IST_OFFSET);
  expect_eq(non_boolean_specs.size(), size_t{6},
            "non-boolean loading_allowed remains included");

  auto invalid_route = make_base_route();
  invalid_route.erase("route_schedule_uuid");
  expect_eq(build_route_edge_specs(invalid_route, IST_OFFSET).empty(), true,
            "missing route fields return no specs");

  auto invalid_stop_route = make_base_route();
  invalid_stop_route["halt_centers"][2].erase("center_code");
  const auto invalid_stop_specs =
    build_route_edge_specs(invalid_stop_route, IST_OFFSET);
  expect_eq(invalid_stop_specs.size(), size_t{1},
            "invalid halt center skips affected edges only");
  expect_eq(invalid_stop_specs[0].source_center_code, std::string{"A"},
            "remaining valid source after invalid stop");
  expect_eq(invalid_stop_specs[0].target_center_code, std::string{"D"},
            "remaining valid target after invalid stop");

  auto negative_duration_route = make_base_route();
  negative_duration_route["halt_centers"][2]["rel_eta"] = "0:30";
  const auto negative_specs =
    build_route_edge_specs(negative_duration_route, IST_OFFSET);
  expect_eq(negative_specs.size(), size_t{2},
            "negative duration skips affected edges");
  expect_eq(negative_specs[0].target_center_code, std::string{"D"},
            "negative duration keeps valid A-D edge");
  expect_eq(negative_specs[1].source_center_code, std::string{"C"},
            "negative duration keeps valid C-D edge");

  auto default_days_route = make_base_route();
  default_days_route["days_of_week"] = moirai::Json::array({"bad", true});
  const auto default_days_specs =
    build_route_edge_specs(default_days_route, IST_OFFSET);
  expect_eq(default_days_specs[0].edge->days_of_week, ALL_DAYS_OF_WEEK,
            "invalid days default to all days");

  auto negative_offset_route = make_base_route();
  negative_offset_route["reporting_time"] = "01:00";
  negative_offset_route["halt_centers"][0]["rel_etd"] = "0:30";
  const auto negative_offset_specs =
    build_route_edge_specs(negative_offset_route, IST_OFFSET);
  expect_eq(negative_offset_specs[0].edge->departure.count(), -240,
            "negative reporting offset is preserved");
}

void test_large_route_edge_spec_expansion() {
  moirai::Json stops = moirai::Json::array();
  for (int index = 0; index < 12; ++index) {
    stops.push_back({
      {"center_code", std::format("N{}", index)},
      {"rel_eta", std::format("{}:00", index * 2)},
      {"rel_etd", std::format("{}:30", index * 2)},
      {"loading_allowed", index % 3 != 0},
    });
  }

  const moirai::Json route = {
    {"route_schedule_uuid", "large-route"},
    {"name", "Large Route"},
    {"route_type", "carting"},
    {"reporting_time", "06:00"},
    {"days_of_week", {1, 2, 3, 4, 5, 6}},
    {"halt_centers", stops},
  };

  const auto started = std::chrono::steady_clock::now();
  const auto specs = build_route_edge_specs(route, IST_OFFSET);
  const auto elapsed = std::chrono::steady_clock::now() - started;

  expect_eq(specs.size(), size_t{28},
            "large route expands N choose 2 after loading filter");
  expect_eq(specs.front().source_center_code, std::string{"N1"},
            "first included source after filtering");
  expect_eq(specs.back().target_center_code, std::string{"N11"},
            "last included target after filtering");
  expect_true(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                  .count() < 1000,
              "large route expansion smoke test stays bounded");
}

} // namespace

auto main() -> int {
  test_graph_basics();
  test_forward_path_selection();
  test_reverse_path_selection();
  test_days_of_week_graph_behavior();
  test_vehicle_filtering();
  test_route_edge_spec_expansion();
  test_large_route_edge_spec_expansion();
  return EXIT_SUCCESS;
}
