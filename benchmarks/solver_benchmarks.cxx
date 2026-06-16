#include <nlohmann/json.hpp>

import std;
import moirai.date_utils;
import moirai.json_utils;
import moirai.route_schedule;
import moirai.solver;
import moirai.transportation;

#ifndef MOIRAI_TEST_FIXTURE_DIR
#define MOIRAI_TEST_FIXTURE_DIR "tests/fixtures"
#endif

namespace {

constexpr auto ITERATIONS = 500;

struct BenchmarkGraph {
  std::shared_ptr<Solver> solver;
  NodeId source{INVALID_NODE};
  NodeId target{INVALID_NODE};
};

struct LoadQuery {
  NodeId source{INVALID_NODE};
  NodeId target{INVALID_NODE};
  CLOCK start{};
};

auto day_mask(const int day) -> std::uint8_t {
  return static_cast<std::uint8_t>(1U << (((day % 7) + 7) % 7));
}

auto add_center(Solver& solver, std::string code) -> NodeId {
  return solver.add_node(TransportCenter{std::move(code)});
}

void add_edge(Solver& solver, NodeId source, NodeId target, std::string code,
              int departure_minutes, int duration_minutes,
              VehicleType vehicle = VehicleType::SURFACE,
              std::uint8_t days = ALL_DAYS_OF_WEEK) {
  const auto movement = vehicle == VehicleType::AIR ? MovementType::LINEHAUL
                                                    : MovementType::CARTING;
  auto edge = TransportEdge(
      std::move(code), "benchmark", DURATION{static_cast<std::int16_t>(
                                   departure_minutes % (24 * 60))},
      DURATION{static_cast<std::int16_t>(duration_minutes)}, DURATION{0},
      DURATION{0}, vehicle, movement, false, days);
  if (solver.add_edge(source, target, std::move(edge)) == INVALID_EDGE) {
    throw std::runtime_error("failed to add benchmark edge");
  }
}

struct OwnedDoc {
  std::unique_ptr<moirai::JsonParser> parser =
    std::make_unique<moirai::JsonParser>();
  std::string source;
  moirai::Json element{};

  static auto from_nlohmann(const nlohmann::json& j) -> OwnedDoc {
    OwnedDoc doc;
    doc.source = j.dump();
    auto parsed = moirai::parse_json(*doc.parser, doc.source);
    if (!parsed.has_value()) {
      throw std::runtime_error("OwnedDoc parse failed");
    }
    doc.element = *parsed;
    return doc;
  }

  static auto from_string(std::string json_str) -> OwnedDoc {
    OwnedDoc doc;
    doc.source = std::move(json_str);
    auto parsed = moirai::parse_json(*doc.parser, doc.source);
    if (!parsed.has_value()) {
      throw std::runtime_error("OwnedDoc parse failed");
    }
    doc.element = *parsed;
    return doc;
  }

  operator const moirai::Json&() const { return element; }
};

auto make_route_fixture(std::string uuid, int stop_count) -> OwnedDoc {
  nlohmann::json stops = nlohmann::json::array();
  for (int index = 0; index < stop_count; ++index) {
    stops.push_back({
      {"center_code", std::format("B{}", index)},
      {"rel_eta", std::format("{}:00", index * 2)},
      {"rel_etd", std::format("{}:30", index * 2)},
      {"loading_allowed", true},
    });
  }

  nlohmann::json route = {
    {"route_schedule_uuid", std::move(uuid)},
    {"name", "Benchmark Route"},
    {"route_type", "carting"},
    {"reporting_time", "06:00"},
    {"days_of_week", {1, 2, 3, 4, 5, 6}},
    {"halt_centers", std::move(stops)},
  };

  return OwnedDoc::from_nlohmann(route);
}

auto make_graph(std::string_view name, int node_count, int fanout)
    -> BenchmarkGraph {
  auto solver = std::make_shared<Solver>();
  std::vector<NodeId> nodes;
  nodes.reserve(static_cast<std::size_t>(node_count));
  for (int index = 0; index < node_count; ++index) {
    nodes.push_back(add_center(*solver, std::format("{}-{}", name, index)));
  }

  for (int source = 0; source < node_count; ++source) {
    for (int offset = 1; offset <= fanout; ++offset) {
      const int target = source + offset;
      if (target >= node_count) {
        break;
      }
      const int departure = (6 * 60) + ((source * 17 + offset * 29) % 900);
      const int duration = 30 + ((source + offset) % 180);
      const auto vehicle =
          offset % 5 == 0 ? VehicleType::AIR : VehicleType::SURFACE;
      const auto days = offset % 7 == 0
                            ? static_cast<std::uint8_t>(day_mask(1) |
                                                        day_mask(3) |
                                                        day_mask(5))
                            : ALL_DAYS_OF_WEEK;
      add_edge(*solver, nodes[source], nodes[target],
               std::format("{}-{}-{}", name, source, target), departure,
               duration, vehicle, days);
    }
  }

  for (int source = 0; source + 97 < node_count; source += 31) {
    const int target = source + 97;
    add_edge(*solver, nodes[source], nodes[target],
             std::format("{}-express-{}-{}", name, source, target), 8 * 60,
             240, VehicleType::AIR, day_mask(0) | day_mask(2) | day_mask(4));
  }

  return {.solver = solver, .source = nodes.front(), .target = nodes.back()};
}

auto make_real_fixture_graph() -> BenchmarkGraph {
  auto solver = std::make_shared<Solver>();
  const auto fixture =
      std::filesystem::path{MOIRAI_TEST_FIXTURE_DIR} / "real_routes.json";
  std::ifstream input(fixture);
  const auto parsed = moirai::parse_json(input);
  if (!parsed.has_value() || !parsed->is_object()) {
    throw std::runtime_error("failed to parse real route fixture");
  }

  const auto* routes = moirai::find_array_member(*parsed, "data");
  if (routes == nullptr || moirai::json_size(*routes) == 0) {
    throw std::runtime_error("real route fixture has no route data");
  }

  std::vector<RouteEdgeSpec> specs;
  specs.reserve(moirai::json_size(*routes) * 2U);
  std::unordered_set<std::string> centers;
  for (const auto& route : *routes) {
    auto route_specs = build_route_edge_specs(route, DURATION{330});
    for (auto& spec : route_specs) {
      centers.insert(spec.source_center_code);
      centers.insert(spec.target_center_code);
      specs.push_back(std::move(spec));
    }
  }
  if (specs.empty()) {
    throw std::runtime_error("real route fixture produced no route edges");
  }

  solver->reserve_nodes(centers.size());
  for (const auto& center : centers) {
    (void)add_center(*solver, center);
  }
  solver->reserve_edges(specs.size());
  for (auto& spec : specs) {
    const auto source = solver->find_node(spec.source_center_code);
    const auto target = solver->find_node(spec.target_center_code);
    if (source.has_value() && target.has_value()) {
      (void)solver->add_edge(*source, *target, std::move(spec.edge));
    }
  }
  solver->finalize_graph();

  const auto source = solver->find_node(specs.front().source_center_code);
  const auto target = solver->find_node(specs.front().target_center_code);
  if (!source.has_value() || !target.has_value()) {
    throw std::runtime_error("real route fixture benchmark endpoints missing");
  }
  return {.solver = solver, .source = *source, .target = *target};
}

auto make_fixture_graph(const std::filesystem::path& fixture) -> BenchmarkGraph {
  auto solver = std::make_shared<Solver>();
  std::ifstream input(fixture);
  const auto parsed = moirai::parse_json(input);
  if (!parsed.has_value() || !parsed->is_object()) {
    throw std::runtime_error(std::format("failed to parse route fixture {}",
                                         fixture.string()));
  }

  const auto* routes = moirai::find_array_member(*parsed, "data");
  if (routes == nullptr || moirai::json_size(*routes) == 0) {
    throw std::runtime_error("route fixture has no data[] routes");
  }

  std::vector<RouteEdgeSpec> specs;
  specs.reserve(moirai::json_size(*routes) * 8U);
  std::unordered_set<std::string> centers;
  for (const auto& route : *routes) {
    auto route_specs = build_route_edge_specs(route, DURATION{330});
    for (auto& spec : route_specs) {
      centers.insert(spec.source_center_code);
      centers.insert(spec.target_center_code);
      specs.push_back(std::move(spec));
    }
  }
  if (specs.empty()) {
    throw std::runtime_error("route fixture produced no route edges");
  }

  solver->reserve_nodes(centers.size());
  for (const auto& center : centers) {
    (void)add_center(*solver, center);
  }
  solver->reserve_edges(specs.size());
  for (auto& spec : specs) {
    const auto source = solver->find_node(spec.source_center_code);
    const auto target = solver->find_node(spec.target_center_code);
    if (source.has_value() && target.has_value()) {
      (void)solver->add_edge(*source, *target, std::move(spec.edge));
    }
  }
  solver->finalize_graph();

  const auto source = solver->find_node(specs.front().source_center_code);
  const auto target = solver->find_node(specs.front().target_center_code);
  if (!source.has_value() || !target.has_value()) {
    throw std::runtime_error("route fixture benchmark endpoints missing");
  }
  return {.solver = solver, .source = *source, .target = *target};
}

auto make_load_queries(const Solver& solver, const std::filesystem::path& fixture)
    -> std::vector<LoadQuery> {
  std::ifstream input(fixture);
  if (!input.is_open()) {
    throw std::runtime_error(std::format("failed to open load fixture {}",
                                         fixture.string()));
  }

  std::vector<LoadQuery> queries;

  const auto extract_from_record = [&](const moirai::Json& record) {
    if (!record.is_object()) {
      return;
    }
    const auto location = moirai::find_string_member(record, "location");
    const auto destination = moirai::find_string_member(record, "destination");
    const auto time = moirai::find_string_member(record, "time");
    if (!location.has_value() || !destination.has_value() || !time.has_value()) {
      return;
    }
    const auto source = solver.find_node(*location);
    const auto target = solver.find_node(*destination);
    if (!source.has_value() || !target.has_value()) {
      return;
    }
    try {
      queries.push_back(LoadQuery{
        .source = *source,
        .target = *target,
        .start = iso_to_date(std::string(*time)),
      });
    } catch (const std::exception&) {
    }
  };

  if (fixture.extension() == ".jsonl") {
    std::string line;
    while (std::getline(input, line)) {
      if (line.empty()) {
        continue;
      }
      auto parsed = moirai::parse_json(line);
      if (parsed.has_value()) {
        extract_from_record(*parsed);
      }
    }
    return queries;
  }

  std::string content(std::istreambuf_iterator<char>(input), {});
  auto parsed = moirai::parse_json(content);
  if (parsed.has_value() && parsed->is_array()) {
    for (const auto& record : *parsed) {
      extract_from_record(record);
    }
  }
  return queries;
}

auto resident_kb() -> std::size_t {
  std::ifstream status("/proc/self/status");
  std::string key;
  while (status >> key) {
    if (key == "VmRSS:") {
      std::size_t value{};
      status >> value;
      return value;
    }
    status.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  }
  return 0;
}

template <typename Fn>
auto run_timed_check(std::string_view name, double max_ms, Fn&& fn) -> bool {
  const auto started = std::chrono::steady_clock::now();
  const auto observed = fn();
  const auto elapsed = std::chrono::steady_clock::now() - started;
  const auto elapsed_ms = static_cast<double>(
      std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()) /
                          1000.0;
  std::println("{}: {} ms (observed={})", name, elapsed_ms, observed);
  if (elapsed_ms > max_ms) {
    std::println(std::cerr, "{}: exceeded {} ms ceiling", name, max_ms);
    return false;
  }
  return true;
}

auto run_build_benchmarks() -> bool {
  bool passed = true;
  passed &= run_timed_check("route-expansion", 1000.0, [] {
    std::size_t edge_count = 0;
    for (int index = 0; index < 25; ++index) {
      const auto route = make_route_fixture(std::format("bench-route-{}", index),
                                            48);
      edge_count += build_route_edge_specs(route, DURATION{330}).size();
    }
    return edge_count;
  });
  passed &= run_timed_check("graph-build-finalize", 5000.0, [] {
    const auto graph = make_graph("build", 1200, 12);
    graph.solver->finalize_graph();
    return graph.solver->graph_stats().edges;
  });
  return passed;
}

template <typename Fn>
auto run_benchmark(std::string_view name, double max_us_per_op,
                   std::optional<std::size_t> expected_solved, Fn&& fn) -> bool {
  std::vector<double> samples;
  samples.reserve(ITERATIONS);
  std::size_t solved = 0;
  for (int iteration = 0; iteration < ITERATIONS; ++iteration) {
    const auto started = std::chrono::steady_clock::now();
    solved += fn().empty() ? 0U : 1U;
    const auto elapsed = std::chrono::steady_clock::now() - started;
    samples.push_back(static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count()) /
                      1000.0);
  }
  std::ranges::sort(samples);
  const auto percentile = [&samples](double pct) -> double {
    const auto index = std::min<std::size_t>(
        samples.size() - 1U,
        static_cast<std::size_t>((pct / 100.0) *
                                 static_cast<double>(samples.size() - 1U)));
    return samples[index];
  };
  const auto total = std::accumulate(samples.begin(), samples.end(), 0.0);
  const double per_op = total / static_cast<double>(samples.size());
  std::println("{}: mean={} us p50={} us p95={} us p99={} us ({} solved)",
               name, per_op, percentile(50.0), percentile(95.0),
               percentile(99.0), solved);

  bool passed = true;
  if (expected_solved.has_value() && solved != *expected_solved) {
    std::println(std::cerr, "{}: expected {} solved paths, observed {}", name,
                 *expected_solved, solved);
    passed = false;
  }
  if (per_op > max_us_per_op) {
    std::println(std::cerr, "{}: exceeded {} us/op ceiling", name,
                 max_us_per_op);
    passed = false;
  }
  return passed;
}

auto run_suite(std::string_view name, const BenchmarkGraph& graph) -> bool {
  const auto stats = graph.solver->graph_stats();
  std::println(
      "{} graph: queue={} nodes={} edges={} csr_out={} csr_in={} avg_out_degree={} "
      "max_out_degree={}",
      name, stats.queue, stats.nodes, stats.edges, stats.outgoing_storage,
      stats.incoming_storage, stats.average_out_degree, stats.max_out_degree);

  const auto monday_0500 = iso_to_date("2026-06-08 05:00:00");
  const auto sunday_1200 = iso_to_date("2026-06-14 12:00:00");
  const auto unreachable = add_center(*graph.solver, std::format("{}-z", name));
  const auto is_small = name == "small";
  const auto is_medium = name == "medium";
  const double reachable_ceiling =
      is_small ? 50.0 : is_medium ? 2000.0 : 15000.0;
  const double unreachable_ceiling =
      is_small ? 50.0 : is_medium ? 2000.0 : 15000.0;

  bool passed = true;
  passed &= run_benchmark(std::format("{}-forward-surface-monday", name),
                          reachable_ceiling, std::size_t{ITERATIONS}, [&] {
    return graph.solver
        ->find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
            graph.source, graph.target, monday_0500);
  });
  passed &= run_benchmark(std::format("{}-forward-air-sunday", name),
                          reachable_ceiling, std::size_t{ITERATIONS}, [&] {
    return graph.solver->find_path<PathTraversalMode::FORWARD, VehicleType::AIR>(
        graph.source, graph.target, sunday_1200);
  });
  passed &= run_benchmark(std::format("{}-reverse-surface-sunday", name),
                          reachable_ceiling, std::size_t{ITERATIONS}, [&] {
    return graph.solver
        ->find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
            graph.target, graph.source, sunday_1200);
  });
  passed &= run_benchmark(std::format("{}-unreachable", name),
                          unreachable_ceiling, std::size_t{0}, [&] {
    return graph.solver
        ->find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
            graph.source, unreachable, monday_0500);
  });
  return passed;
}

auto run_production_fixture_suite() -> bool {
  const char* routes_fixture = std::getenv("MOIRAI_BENCH_ROUTES_FIXTURE");
  if (routes_fixture == nullptr || std::string_view{routes_fixture}.empty()) {
    return true;
  }

  bool passed = true;
  BenchmarkGraph graph;
  passed &= run_timed_check("production-graph-build-finalize", 120000.0, [&] {
    graph = make_fixture_graph(routes_fixture);
    return graph.solver->graph_stats().edges;
  });
  const auto stats = graph.solver->graph_stats();
  std::println(
      "production graph: queue={} nodes={} edges={} csr_out={} csr_in={} "
      "avg_out_degree={} max_out_degree={} rss_kb={}",
      stats.queue, stats.nodes, stats.edges, stats.outgoing_storage,
      stats.incoming_storage, stats.average_out_degree, stats.max_out_degree,
      resident_kb());
  passed &= run_suite("production", graph);

  const char* loads_fixture = std::getenv("MOIRAI_BENCH_LOADS_FIXTURE");
  if (loads_fixture != nullptr && !std::string_view{loads_fixture}.empty()) {
    const auto queries = make_load_queries(*graph.solver, loads_fixture);
    std::println("production loads: usable_queries={}", queries.size());
    if (!queries.empty()) {
      std::size_t index = 0;
      passed &= run_benchmark("production-load-forward-surface",
                              20000.0,
                              std::nullopt,
                              [&]() {
        const auto& query = queries[index++ % queries.size()];
        return graph.solver
          ->find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
            query.source, query.target, query.start);
      });
    }
  }
  return passed;
}

} // namespace

auto main() -> int {
  bool passed = true;
  passed &= run_build_benchmarks();
  passed &= run_suite("small", make_graph("small", 16, 4));
  passed &= run_suite("medium", make_graph("medium", 600, 10));
  passed &= run_suite("large", make_graph("large", 2400, 16));
  passed &= run_suite("real", make_real_fixture_graph());
  passed &= run_production_fixture_suite();
  return passed ? 0 : 1;
}
