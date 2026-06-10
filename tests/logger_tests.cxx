#include "test_helpers.hxx"
#include <cstddef>
#include <string>

namespace {

using moirai_tests::ScopedLogCapture;
using moirai_tests::expect_eq;
using moirai_tests::expect_true;

void test_logger_capture_and_levels() {
  auto& logger = moirai::Application::instance().logger();
  logger.set_level("information");
  {
    ScopedLogCapture logs;
    logger.debug("hidden debug");
    logger.error("visible error");
    expect_eq(logs.records.size(), size_t{1}, "debug suppressed by info level");
    expect_true(logs.contains("visible error"), "error captured");
    expect_eq(logs.records[0].label, std::string{"ERROR"}, "error label");
  }

  logger.set_level("debug");
  {
    ScopedLogCapture logs;
    logger.debug("visible debug");
    logger.information("visible info");
    logger.error("visible error");
    expect_eq(logs.records.size(), size_t{3}, "debug level captures all logs");
    expect_true(logs.contains("visible debug"), "debug captured");
    expect_true(logs.contains("visible info"), "info captured");
    expect_true(logs.contains("visible error"), "error captured");
  }
  logger.set_level("information");
}

} // namespace

auto main() -> int {
  test_logger_capture_and_levels();
  return EXIT_SUCCESS;
}
