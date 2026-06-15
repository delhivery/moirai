#pragma once

#include "blocking_queue.hxx"

import std;
import moirai.date_utils;
import moirai.app;
import moirai.http;
import moirai.search_document;
import moirai.solver;
import moirai.solver_wrapper;

#ifndef MOIRAI_TEST_FIXTURE_DIR
#define MOIRAI_TEST_FIXTURE_DIR "tests/fixtures"
#endif

namespace moirai_tests {

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

inline void expect_true(bool value, std::string_view label,
                        std::source_location location =
                          std::source_location::current()) {
  expect_eq(value, true, label, location);
}

inline auto fixture_path(std::string_view name) -> std::filesystem::path {
  return std::filesystem::path{MOIRAI_TEST_FIXTURE_DIR} / name;
}

inline auto read_fixture(std::string_view name) -> std::string {
  std::ifstream input(fixture_path(name));
  if (!input.is_open()) {
    std::cerr << "Failed to open fixture " << name << '\n';
    std::exit(1);
  }

  std::ostringstream output;
  output << input.rdbuf();
  return output.str();
}

struct FakeHttp {
  std::map<std::string, moirai::HttpResponse> responses;

  auto operator()(const moirai::Uri& uri,
                  const std::vector<std::string>& headers) const
      -> moirai::HttpResponse {
    (void)headers;
    const auto key = uri.path_and_query();
    const auto found = responses.find(key);
    if (found == responses.end()) {
      return {.status_code = 404, .body = key};
    }
    return found->second;
  }
};

struct LogRecord {
  moirai::LogLevel level;
  std::string label;
  std::string message;
};

class ScopedLogCapture {
public:
  ScopedLogCapture() {
    moirai::Application::instance().logger().set_sink(
      [this](moirai::LogLevel level,
             std::string_view label,
             std::string_view message) {
        records.push_back(LogRecord{
          .level = level,
          .label = std::string(label),
          .message = std::string(message),
        });
      });
  }

  ScopedLogCapture(const ScopedLogCapture&) = delete;
  auto operator=(const ScopedLogCapture&) -> ScopedLogCapture& = delete;

  ~ScopedLogCapture() {
    moirai::Application::instance().logger().clear_sink();
  }

  [[nodiscard]] auto contains(std::string_view text) const -> bool {
    return std::ranges::any_of(records, [text](const LogRecord& record) {
      return record.message.find(text) != std::string::npos;
    });
  }

  [[nodiscard]] auto count_containing(std::string_view text) const -> std::size_t {
    return static_cast<std::size_t>(
      std::ranges::count_if(records, [text](const LogRecord& record) {
        return record.message.find(text) != std::string::npos;
      }));
  }

  std::vector<LogRecord> records;
};

inline auto default_fake_http() -> FakeHttp {
  return FakeHttp{{
    {"/facilities?page=1&status=active",
     {.status_code = 200, .body = read_fixture("facilities_page1.json")}},
    {"/facilities?page=2&status=active",
     {.status_code = 200, .body = read_fixture("facilities_page2.json")}},
    {"/routes", {.status_code = 200, .body = read_fixture("routes.json")}},
  }};
}

struct WrapperHarness {
  BlockingQueue<std::string> node_queue;
  BlockingQueue<std::string> edge_queue;
  BlockingQueue<std::string> load_queue;
  BlockingQueue<SearchDocument> solution_queue;
  std::shared_ptr<Solver> solver = std::make_shared<Solver>();

  [[nodiscard]] auto queues() -> SolverWrapper::RuntimeQueues {
    return {
      .node = &node_queue,
      .edge = &edge_queue,
      .load = &load_queue,
      .solution = &solution_queue,
    };
  }
};

inline auto endpoints() -> SolverWrapper::InitEndpoints {
  return {
    .node_uri = "http://test/facilities",
    .node_token = "node-token",
    .edge_uri = "http://test/routes",
    .edge_token = "edge-token",
  };
}

inline auto epoch_minutes(std::string_view timestamp) -> int32_t {
  return static_cast<int32_t>(iso_to_date(std::string(timestamp))
                                .time_since_epoch()
                                .count());
}

} // namespace moirai_tests
