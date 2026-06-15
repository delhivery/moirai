#include "test_helpers.hxx"

import std;
import moirai.date_utils;
import moirai.processor;
import moirai.solver;
import moirai.transportation;

namespace {

using moirai_tests::display_epoch_minutes;
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

struct OwnedPath {
  std::shared_ptr<TransportCenter> first;
  std::shared_ptr<TransportCenter> second;
  std::shared_ptr<TransportEdge> outbound;
  Path path;
};

auto two_segment_path() -> OwnedPath {
  OwnedPath owned;
  owned.first = center("A");
  owned.second = center("B");
  owned.outbound = edge("route.uuid.0", 9 * 60, 60);
  owned.path.steps = {
    PathStep{
      .node = owned.first.get(),
      .outbound = owned.outbound.get(),
      .distance = iso_to_date("2026-06-08 08:00:00"),
    },
    PathStep{
      .node = owned.second.get(),
      .outbound = nullptr,
      .distance = iso_to_date("2026-06-08 10:00:00"),
    },
  };
  return owned;
}

void expect_timestamp_matches_display(std::string_view display,
                                      std::int64_t timestamp,
                                      std::string_view label) {
  expect_eq(timestamp,
            static_cast<std::int64_t>(display_epoch_minutes(display)),
            label);
}

void test_forward_parse_path_shape() {
  const auto owned = two_segment_path();
  const auto parsed = parse_path<PathTraversalMode::FORWARD>(owned.path);
  expect_eq(parsed.size(), std::size_t{2}, "forward parse path size");
  expect_eq(parsed[0].code, std::string{"A"}, "forward first code");
  expect_eq(owned.outbound->route_prefix, std::string{"route"},
            "route prefix precomputed");
  expect_eq(parsed[0].route, std::string{"route"}, "forward route prefix");
  expect_true(parsed[0].has_departure, "forward has departure");
  expect_timestamp_matches_display(parsed[0].arrival,
                                   parsed[0].arrival_ts,
                                   "forward first arrival timestamp");
  expect_timestamp_matches_display(parsed[0].departure,
                                   parsed[0].departure_ts,
                                   "forward first departure timestamp");
  expect_eq(parsed[1].code, std::string{"B"}, "forward terminal code");
  expect_eq(parsed[1].has_departure, false, "terminal has no route");
  expect_timestamp_matches_display(parsed[1].arrival,
                                   parsed[1].arrival_ts,
                                   "forward terminal arrival timestamp");

  std::vector<SearchPathLocation> parsed_into;
  parse_path_into<PathTraversalMode::FORWARD>(owned.path, parsed_into);
  expect_eq(parsed_into.size(), parsed.size(), "forward parse into size");
  expect_eq(parsed_into[0].route, parsed[0].route,
            "forward parse into route");
}

void test_reverse_parse_path_shape() {
  const auto owned = two_segment_path();
  const auto parsed = parse_path<PathTraversalMode::REVERSE>(owned.path);
  expect_eq(parsed.size(), std::size_t{2}, "reverse parse path size");
  expect_eq(parsed[0].code, std::string{"A"}, "reverse first code");
  expect_eq(parsed[0].route, std::string{"route"}, "reverse route prefix");
  expect_true(!parsed[0].arrival.empty(), "reverse has arrival");
  expect_true(parsed[0].has_departure, "reverse has departure");
  expect_timestamp_matches_display(parsed[0].arrival,
                                   parsed[0].arrival_ts,
                                   "reverse first arrival timestamp");
  expect_timestamp_matches_display(parsed[0].departure,
                                   parsed[0].departure_ts,
                                   "reverse first departure timestamp");
  expect_eq(parsed[1].code, std::string{"B"}, "reverse terminal code");
  expect_eq(parsed[1].has_departure, false, "reverse terminal has no route");
  expect_timestamp_matches_display(parsed[1].arrival,
                                   parsed[1].arrival_ts,
                                   "reverse terminal arrival timestamp");

  std::vector<SearchPathLocation> parsed_into;
  parse_path_into<PathTraversalMode::REVERSE>(owned.path, parsed_into);
  expect_eq(parsed_into.size(), parsed.size(), "reverse parse into size");
  expect_eq(parsed_into[0].route, parsed[0].route,
            "reverse parse into route");
}

void test_null_path_serializes_empty() {
  expect_eq(parse_path<PathTraversalMode::FORWARD>(Path{}).empty(), true,
            "empty forward path is empty");
  expect_eq(parse_path<PathTraversalMode::REVERSE>(Path{}).empty(), true,
            "empty reverse path is empty");
}

} // namespace

auto main() -> int {
  test_forward_parse_path_shape();
  test_reverse_parse_path_shape();
  test_null_path_serializes_empty();
  return 0;
}
