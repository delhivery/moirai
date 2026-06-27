#include "test_helpers.hxx"

import std;

namespace {

using moirai_tests::WrapperHarness;
using moirai_tests::ScopedLogCapture;
using moirai_tests::default_fake_http;
using moirai_tests::endpoints;
using moirai_tests::expect_eq;
using moirai_tests::expect_true;
using moirai_tests::fixture_path;

void test_init_timings_legacy_file_is_ignored() {
  WrapperHarness harness;
  SolverWrapper missing_file_wrapper(harness.queues(),
                                     harness.solver,
                                     fixture_path("missing-timings.json"));

  const auto bad_path =
    std::filesystem::temp_directory_path() / "moirae-bad-timings.json";
  {
    std::ofstream output(bad_path);
    output << "{}";
  }

  SolverWrapper bad_file_wrapper(harness.queues(), harness.solver, bad_path);
  expect_true(true, "legacy timings file is compatibility-only");
}

void test_endpoint_initialization_builds_graph() {
  WrapperHarness harness;
  ScopedLogCapture logs;
  SolverWrapper wrapper(harness.queues(),
                        endpoints(),
                        fixture_path("timings.json"),
                        default_fake_http());

  const auto solver = wrapper.get_solver();
  expect_eq(solver->show(), std::string{"Graph<5, 6>"},
            "init builds facilities custody and route edges");

  const auto a = solver->find_node("A");
  expect_true(a.has_value(), "facility A exists");
  const auto center_a = solver->get_node(*a);
  expect_eq(center_a->get_latency<MovementType::CARTING,
                                  ProcessType::INBOUND>().count(),
            0, "facility API leaves inbound processing on route edges");
  expect_eq(center_a->get_latency<MovementType::CARTING,
                                  ProcessType::OUTBOUND>().count(),
            10, "facility API parses outbound processing");
  expect_eq(center_a->get_fresh_processing_time().count(), 25,
            "facility API parses fresh shipment processing");
  expect_eq(center_a->get_mixed_bag_processing_time().count(), 40,
            "facility API parses mixed bag processing");
  expect_eq(center_a->get_cutoff().count(), 360,
            "facility API normalizes arrival cutoff to UTC");

  const auto e = solver->find_node("E");
  expect_true(e.has_value(), "facility without timing exists");
  expect_eq(solver->get_node(*e)->get_cutoff().count(), 240,
            "default cutoff used for missing timing record");

  const auto graph_dump = solver->show_all();
  expect_true(graph_dump.find("CUSTODY-A-B: A TO B") != std::string::npos,
              "custody edge A-B exists");
  expect_true(graph_dump.find("CUSTODY-B-A: B TO A") != std::string::npos,
              "custody edge B-A exists");
  expect_true(graph_dump.find("route-ac.0: A TO C") != std::string::npos,
              "route edge A-C exists");
  expect_true(graph_dump.find("route-ac.0: A TO B") == std::string::npos,
              "loading disallowed B is not a route endpoint");
  expect_true(graph_dump.find("route-missing-node") == std::string::npos,
              "route with missing target node is skipped");
  expect_true(logs.contains("Skipping facility without facility_code"),
              "missing facility_code logged");
  expect_true(logs.contains("Edge<route-missing-node.0>"),
              "missing route node logged");
}

void test_initialization_handles_bad_http_responses() {
  WrapperHarness harness;
  ScopedLogCapture logs;
  moirai_tests::FakeHttp bad_http{{
    {"/facilities?page=1&status=active", {.status_code = 500, .body = "bad"}},
    {"/routes", {.status_code = 500, .body = "bad"}},
  }};
  SolverWrapper wrapper(harness.queues(),
                        endpoints(),
                        fixture_path("timings.json"),
                        bad_http);
  expect_eq(wrapper.get_solver()->show(), std::string{"Graph<0, 0>"},
            "non-200 init responses leave graph empty");
  expect_true(logs.contains("Unable to fetch facility data"),
              "non-200 facility response logged");
  expect_true(logs.contains("Unable to fetch route data"),
              "non-200 route response logged");

  WrapperHarness invalid_json_harness;
  ScopedLogCapture invalid_logs;
  moirai_tests::FakeHttp invalid_http{{
    {"/facilities?page=1&status=active",
     {.status_code = 200, .body = "{\"result\":{}}"}},
    {"/routes", {.status_code = 200, .body = "{\"not_data\":[]}"}},
  }};
  SolverWrapper invalid_wrapper(invalid_json_harness.queues(),
                                endpoints(),
                                fixture_path("timings.json"),
                                invalid_http);
  expect_eq(invalid_wrapper.get_solver()->show(), std::string{"Graph<0, 0>"},
            "malformed init responses leave graph empty");
  expect_true(invalid_logs.contains(
                "Facility response is missing result/data metadata"),
              "missing facility metadata logged");
  expect_true(invalid_logs.contains("Route response is missing data array"),
              "missing route data logged");
}

void test_initialization_handles_invalid_json_logs() {
  WrapperHarness harness;
  ScopedLogCapture logs;
  moirai_tests::FakeHttp invalid_http{{
    {"/facilities?page=1&status=active",
     {.status_code = 200, .body = "not-json"}},
    {"/routes", {.status_code = 200, .body = "not-json"}},
  }};

  SolverWrapper wrapper(harness.queues(),
                        endpoints(),
                        fixture_path("timings.json"),
                        invalid_http);
  expect_eq(wrapper.get_solver()->show(), std::string{"Graph<0, 0>"},
            "invalid JSON responses leave graph empty");
  expect_true(logs.contains("Unable to parse facility response"),
              "invalid facility JSON logged");
  expect_true(logs.contains("Unable to parse route response"),
              "invalid route JSON logged");
}

} // namespace

auto main() -> int {
  test_init_timings_legacy_file_is_ignored();
  test_endpoint_initialization_builds_graph();
  test_initialization_handles_bad_http_responses();
  test_initialization_handles_invalid_json_logs();
  return 0;
}
