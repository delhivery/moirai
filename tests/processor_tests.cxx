#include "date_utils.hxx"
#include "processor.hxx"
#include "test_helpers.hxx"
#include "transportation.hxx"
#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace {

using moirai_tests::expect_eq;
using moirai_tests::expect_true;

auto center(std::string code) -> std::shared_ptr<TransportCenter> {
  return std::make_shared<TransportCenter>(std::move(code));
}

auto edge(std::string code, int departure, int duration)
    -> std::shared_ptr<TransportEdge> {
  return std::make_shared<TransportEdge>(
    std::move(code),
    "route",
    DURATION{static_cast<std::int16_t>(departure)},
    DURATION{static_cast<std::int16_t>(duration)},
    DURATION{0},
    DURATION{0},
    VehicleType::SURFACE,
    MovementType::CARTING,
    false);
}

auto two_segment_path() -> std::shared_ptr<Segment> {
  auto first = std::make_shared<Segment>();
  auto second = std::make_shared<Segment>();
  first->node = center("A");
  first->outbound = edge("route.uuid.0", 9 * 60, 60);
  first->distance = iso_to_date("2026-06-08 08:00:00");
  first->next = second;

  second->node = center("B");
  second->prev = first;
  second->distance = iso_to_date("2026-06-08 10:00:00");
  return first;
}

void test_forward_parse_path_shape() {
  const auto parsed =
    parse_path<PathTraversalMode::FORWARD>(two_segment_path());
  expect_eq(parsed.size(), size_t{2}, "forward parse path size");
  expect_eq(parsed[0]["code"].get<std::string>(), std::string{"A"},
            "forward first code");
  expect_eq(parsed[0]["route"].get<std::string>(), std::string{"route"},
            "forward route prefix");
  expect_true(parsed[0].contains("departure"), "forward has departure");
  expect_true(parsed[0].contains("departure_ts"), "forward has departure ts");
  expect_eq(parsed[1]["code"].get<std::string>(), std::string{"B"},
            "forward terminal code");
  expect_eq(parsed[1].contains("route"), false, "terminal has no route");
}

void test_reverse_parse_path_shape() {
  const auto parsed =
    parse_path<PathTraversalMode::REVERSE>(two_segment_path());
  expect_eq(parsed.size(), size_t{2}, "reverse parse path size");
  expect_eq(parsed[0]["code"].get<std::string>(), std::string{"A"},
            "reverse first code");
  expect_eq(parsed[0]["route"].get<std::string>(), std::string{"route"},
            "reverse route prefix");
  expect_true(parsed[0].contains("arrival"), "reverse has arrival");
  expect_true(parsed[0].contains("departure"), "reverse has departure");
  expect_eq(parsed[1]["code"].get<std::string>(), std::string{"B"},
            "reverse terminal code");
  expect_eq(parsed[1].contains("route"), false, "reverse terminal has no route");
}

void test_null_path_serializes_empty() {
  expect_eq(parse_path<PathTraversalMode::FORWARD>(nullptr).empty(), true,
            "null forward path is empty");
  expect_eq(parse_path<PathTraversalMode::REVERSE>(nullptr).empty(), true,
            "null reverse path is empty");
}

} // namespace

auto main() -> int {
  test_forward_parse_path_shape();
  test_reverse_parse_path_shape();
  test_null_path_serializes_empty();
  return EXIT_SUCCESS;
}
