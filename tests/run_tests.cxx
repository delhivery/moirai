#include <cstdlib>
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
using moirai_tests::read_fixture;

struct RunResult {
  std::vector<SearchDocument> outputs;
  std::vector<moirai_tests::LogRecord> logs;

  [[nodiscard]] auto contains_log(std::string_view text) const -> bool {
    return std::ranges::any_of(logs, [text](const auto& record) {
      return record.message.find(text) != std::string::npos;
    });
  }
};

class ScopedEnv {
public:
  ScopedEnv(const char* name, const char* value) : m_name(name) {
    if (const char* previous = std::getenv(name); previous != nullptr) {
      m_previous = previous;
    }
    if (::setenv(name, value, 1) != 0) {
      std::cerr << "Failed to set environment variable " << name << '\n';
      std::exit(1);
    }
  }

  ScopedEnv(const ScopedEnv&) = delete;
  auto operator=(const ScopedEnv&) -> ScopedEnv& = delete;

  ~ScopedEnv() {
    if (m_previous.has_value()) {
      (void)::setenv(m_name, m_previous->c_str(), 1);
    } else {
      (void)::unsetenv(m_name);
    }
  }

private:
  const char* m_name;
  std::optional<std::string> m_previous;
};

auto run_single_payload(std::string payload,
                        const char* route_expansion_threads = nullptr)
    -> RunResult {
  WrapperHarness harness;
  ScopedLogCapture logs;
  std::optional<ScopedEnv> thread_override;
  if (route_expansion_threads != nullptr) {
    thread_override.emplace("MOIRAI_ROUTE_EXPANSION_THREADS",
                            route_expansion_threads);
  }
  SolverWrapper wrapper(harness.queues(),
                        endpoints(),
                        fixture_path("timings.json"),
                        default_fake_http());
  harness.load_queue.enqueue(std::move(payload));
  harness.load_queue.close();
  wrapper.run(std::stop_token{});

  std::array<SearchDocument, 4> output;
  const auto count = harness.solution_queue.try_dequeue_bulk(output.data(),
                                                             output.size());
  return {
    .outputs =
      {output.begin(), output.begin() + static_cast<std::ptrdiff_t>(count)},
    .logs = logs.records,
  };
}

void test_valid_payload_produces_solution() {
  const auto result = run_single_payload(read_fixture("load_normal.json"));
  expect_eq(result.outputs.size(), std::size_t{1},
            "valid payload produces one solution");
  const auto& solution = result.outputs[0];
  expect_eq(solution.waybill, std::string{"bag-normal"}, "solution waybill");
  expect_true(!solution.earliest.locations.empty(),
              "solution contains earliest");
  expect_eq(solution.cs_slid, std::string{"SLID"}, "cs_slid copied");
  expect_eq(solution.cs_act, std::string{"ACT"}, "cs_act copied");
  expect_eq(solution.pid, std::string{"PID"}, "pid copied");
}

void test_invalid_payloads_produce_no_solution() {
  const auto invalid_json = run_single_payload("not-json");
  expect_eq(invalid_json.outputs.empty(), true,
            "invalid JSON produces no solution");
  expect_true(invalid_json.contains_log("Invalid load payload"),
              "invalid JSON is logged");

  const auto missing_fields = run_single_payload(R"({"id":"missing"})");
  expect_eq(missing_fields.outputs.empty(), true,
            "missing mandatory fields produce no solution");
  expect_true(missing_fields.contains_log(
                "Load payload is invalid: missing id, location, destination, or time"),
              "missing mandatory fields logged");

  const auto same_source = run_single_payload(R"({
    "id":"same",
    "location":"A",
    "destination":"A",
    "time":"2026-06-08 08:00:00"
  })");
  expect_eq(same_source.outputs.empty(), true,
            "same source and destination produce no solution");

  const auto bad_time = run_single_payload(R"({
    "id":"bad-time",
    "location":"A",
    "destination":"C",
    "time":"not-a-date"
  })");
  expect_eq(bad_time.outputs.empty(), true,
            "invalid load time produces no solution");
  expect_true(bad_time.contains_log("Failed to process load bad-time"),
              "invalid load time logged");
}

void test_waybill_items_are_filtered_and_used() {
  const auto result = run_single_payload(read_fixture("load_with_items.json"));
  expect_eq(result.outputs.size(), std::size_t{1},
            "payload with items produces solution");
  expect_true(result.contains_log(
                "Load bag-items contains invalid waybill entry"),
              "invalid waybill fields logged");
  const auto& solution = result.outputs[0];
  expect_eq(solution.package_id, std::string{"child-1"},
            "valid child waybill selected");
  expect_eq(solution.cs_slid, std::string{}, "missing cs_slid defaults empty");
  expect_eq(solution.cs_act, std::string{}, "missing cs_act defaults empty");
  expect_eq(solution.pid, std::string{}, "missing pid defaults empty");
}

void test_item_alias_is_supported() {
  const auto payload = R"({
    "id":"bag-item",
    "location":"A",
    "destination":"C",
    "time":"2026-06-08 08:00:00",
    "item":[
      {"id":"child-alias","cn":"D","ipdd_destination":"2026-06-08 12:00:00"}
    ]
  })";
  const auto result = run_single_payload(payload);
  expect_eq(result.outputs.size(), std::size_t{1}, "item alias produces solution");
  const auto& solution = result.outputs[0];
  expect_eq(solution.package_id, std::string{"child-alias"},
            "item alias child selected");
}

void test_invalid_waybill_date_is_logged_and_skipped() {
  const auto payload = R"({
    "id":"bag-bad-waybill-date",
    "location":"A",
    "destination":"C",
    "time":"2026-06-08 08:00:00",
    "items":[
      {"id":"bad-date","cn":"D","ipdd_destination":"bad-date"}
    ]
  })";
  const auto result = run_single_payload(payload);
  expect_eq(result.outputs.size(), std::size_t{1},
            "bad waybill date does not fail whole load");
  expect_true(result.contains_log(
                "Failed to parse waybill"),
              "bad waybill date logged");
}

void test_route_expansion_thread_override_is_deterministic() {
  const auto payload = read_fixture("load_normal.json");
  const auto single_thread = run_single_payload(payload, "1");
  const auto multi_thread = run_single_payload(payload, "2");

  expect_eq(single_thread.outputs.size(), std::size_t{1},
            "single-thread route expansion produces solution");
  expect_eq(multi_thread.outputs.size(), std::size_t{1},
            "multi-thread route expansion produces solution");
  expect_eq(single_thread.outputs[0].earliest.locations.size(),
            multi_thread.outputs[0].earliest.locations.size(),
            "route expansion thread count preserves path size");
  expect_eq(single_thread.outputs[0].earliest.locations.front().code,
            multi_thread.outputs[0].earliest.locations.front().code,
            "route expansion thread count preserves first path node");
  expect_eq(single_thread.outputs[0].earliest.locations.back().code,
            multi_thread.outputs[0].earliest.locations.back().code,
            "route expansion thread count preserves last path node");
  expect_true(multi_thread.contains_log("Initialized graph: queue="),
              "startup graph stats are logged after finalize");
  expect_true(multi_thread.contains_log("Startup timings:"),
              "startup timing summary is logged");
}

void test_path_cache_hits_repeated_loads() {
  WrapperHarness harness;
  ScopedLogCapture logs;
  SolverWrapper wrapper(harness.queues(),
                        endpoints(),
                        fixture_path("timings.json"),
                        default_fake_http());
  const auto payload = read_fixture("load_normal.json");
  harness.load_queue.enqueue(payload);
  harness.load_queue.enqueue(payload);
  harness.load_queue.close();
  wrapper.run(std::stop_token{});

  std::array<SearchDocument, 4> output;
  const auto count = harness.solution_queue.try_dequeue_bulk(output.data(),
                                                             output.size());
  expect_eq(count, std::size_t{2}, "repeated loads produce two solutions");
  expect_true(logs.contains("Path cache metrics: enabled=true"),
              "path cache metrics logged");
  expect_true(logs.contains("hits="), "path cache hit count logged");
}

} // namespace

auto main() -> int {
  test_valid_payload_produces_solution();
  test_invalid_payloads_produce_no_solution();
  test_waybill_items_are_filtered_and_used();
  test_item_alias_is_supported();
  test_invalid_waybill_date_is_logged_and_skipped();
  test_route_expansion_thread_override_is_deterministic();
  test_path_cache_hits_repeated_loads();
  return 0;
}
