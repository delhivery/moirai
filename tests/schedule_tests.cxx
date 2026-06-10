#include "date_utils.hxx"
#include "json_utils.hxx"
#include "route_schedule.hxx"
#include "transportation.hxx"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <source_location>
#include <string_view>

namespace {

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

void test_route_days_of_week_parsing() {
  expect_eq(parse_route_days_of_week(moirai::Json::object()), ALL_DAYS_OF_WEEK,
            "missing days default to all days");
  expect_eq(parse_route_days_of_week({{"days_of_week", moirai::Json::array()}}),
            ALL_DAYS_OF_WEEK, "empty days default to all days");

  const auto monday_to_saturday =
      parse_route_days_of_week({{"days_of_week", {1, 2, 3, 4, 5, 6}}});
  expect_eq(monday_to_saturday,
            static_cast<std::uint8_t>(ALL_DAYS_OF_WEEK & ~day_mask(0)),
            "Monday to Saturday mask");

  expect_eq(parse_route_days_of_week({{"days_of_week", {0, 7, 14}}}),
            day_mask(0), "Sunday aliases normalize to Sunday");
  expect_eq(parse_route_days_of_week({{"days_of_week", {-1, "8", "bad"}}}),
            static_cast<std::uint8_t>(day_mask(6) | day_mask(1)),
            "negative and string days normalize");
  expect_eq(parse_route_days_of_week({{"days_of_week", {"bad", true, {}}}}),
            ALL_DAYS_OF_WEEK, "fully invalid days default to all days");
}

void test_forward_schedule_traversal() {
  CalcualateTraversalCost calculator;
  const COST monday_0900{
      .schedule_offset = DURATION{9 * 60},
      .duration = DURATION{30},
      .days_of_week = day_mask(1),
  };

  const auto monday_0800 = iso_to_date("2026-06-08 08:00:00");
  expect_eq(minutes_between(
                calculator.operator()<PathTraversalMode::FORWARD>(
                    monday_0800, monday_0900),
                monday_0800),
            90, "forward same-day scheduled route");

  const auto monday_1000 = iso_to_date("2026-06-08 10:00:00");
  expect_eq(minutes_between(
                calculator.operator()<PathTraversalMode::FORWARD>(
                    monday_1000, monday_0900),
                monday_1000),
            (6 * 24 * 60) + (23 * 60) + 30,
            "forward rolls to next allowed week");

  const COST monday_to_saturday_0900{
      .schedule_offset = DURATION{9 * 60},
      .duration = DURATION{15},
      .days_of_week =
          static_cast<std::uint8_t>(ALL_DAYS_OF_WEEK & ~day_mask(0)),
  };
  const auto sunday_0800 = iso_to_date("2026-06-07 08:00:00");
  expect_eq(minutes_between(
                calculator.operator()<PathTraversalMode::FORWARD>(
                    sunday_0800, monday_to_saturday_0900),
                sunday_0800),
            (25 * 60) + 15, "forward skips unscheduled Sunday");

  const COST monday_negative_offset{
      .schedule_offset = DURATION{-150},
      .duration = DURATION{30},
      .days_of_week = day_mask(1),
  };
  const auto sunday_2000 = iso_to_date("2026-06-07 20:00:00");
  expect_eq(minutes_between(
                calculator.operator()<PathTraversalMode::FORWARD>(
                    sunday_2000, monday_negative_offset),
                sunday_2000),
            120, "forward handles UTC-negative local Monday offset");

  const COST monday_25h_offset{
      .schedule_offset = DURATION{25 * 60},
      .duration = DURATION{10},
      .days_of_week = day_mask(1),
  };
  const auto tuesday_midnight = iso_to_date("2026-06-09 00:00:00");
  expect_eq(minutes_between(
                calculator.operator()<PathTraversalMode::FORWARD>(
                    tuesday_midnight, monday_25h_offset),
                tuesday_midnight),
            70, "forward handles offsets beyond one day");
}

void test_reverse_schedule_traversal() {
  CalcualateTraversalCost calculator;
  const COST monday_0900{
      .schedule_offset = DURATION{9 * 60},
      .duration = DURATION{30},
      .days_of_week = day_mask(1),
  };

  const auto monday_1200 = iso_to_date("2026-06-08 12:00:00");
  expect_eq(minutes_between(
                monday_1200,
                calculator.operator()<PathTraversalMode::REVERSE>(
                    monday_1200, monday_0900)),
            210, "reverse same-day scheduled route");

  const auto monday_0830 = iso_to_date("2026-06-08 08:30:00");
  expect_eq(minutes_between(
                monday_0830,
                calculator.operator()<PathTraversalMode::REVERSE>(
                    monday_0830, monday_0900)),
            7 * 24 * 60, "reverse rolls to previous allowed week");

  const COST monday_to_saturday_0900{
      .schedule_offset = DURATION{9 * 60},
      .duration = DURATION{15},
      .days_of_week =
          static_cast<std::uint8_t>(ALL_DAYS_OF_WEEK & ~day_mask(0)),
  };
  const auto sunday_0800 = iso_to_date("2026-06-07 08:00:00");
  expect_eq(minutes_between(
                sunday_0800,
                calculator.operator()<PathTraversalMode::REVERSE>(
                    sunday_0800, monday_to_saturday_0900)),
            (23 * 60) + 15, "reverse skips unscheduled Sunday");

  const COST unreachable{.unreachable = true};
  expect_eq(calculator.operator()<PathTraversalMode::FORWARD>(monday_1200,
                                                              unreachable),
            monday_1200, "forward unreachable cost is identity");
  expect_eq(calculator.operator()<PathTraversalMode::REVERSE>(monday_1200,
                                                              COST{}),
            monday_1200, "reverse zero-day cost is identity");
}

void test_transport_edge_weights() {
  auto source = std::make_shared<TransportCenter>("source");
  auto target = std::make_shared<TransportCenter>("target");
  source->set_latency<MovementType::CARTING, ProcessType::OUTBOUND>(
      DURATION{20});
  target->set_latency<MovementType::CARTING, ProcessType::INBOUND>(
      DURATION{30});

  TransportEdge edge("edge", "route", DURATION{600}, DURATION{120},
                     DURATION{40}, DURATION{60}, VehicleType::SURFACE,
                     MovementType::CARTING, false, day_mask(2));
  edge.update(source, target);

  const auto forward = edge.weight<PathTraversalMode::FORWARD>();
  expect_eq(forward.schedule_offset.count(), 560,
            "forward source offset subtracts outbound latency and loading");
  expect_eq(forward.duration.count(), 220,
            "forward duration includes source and target offsets");
  expect_eq(forward.days_of_week, day_mask(2), "forward preserves day mask");

  const auto reverse = edge.weight<PathTraversalMode::REVERSE>();
  expect_eq(reverse.schedule_offset.count(), 780,
            "reverse offset uses target arrival event");
  expect_eq(reverse.duration.count(), 220,
            "reverse duration includes source and target offsets");
  expect_eq(reverse.days_of_week, day_mask(2), "reverse preserves day mask");

  TransportEdge terminal_edge("terminal", "route", DURATION{600},
                              DURATION{120}, DURATION{40}, DURATION{60},
                              VehicleType::SURFACE, MovementType::CARTING,
                              true, day_mask(3));
  terminal_edge.update(source, target);
  const auto terminal_reverse =
      terminal_edge.weight<PathTraversalMode::REVERSE>();
  expect_eq(terminal_reverse.schedule_offset.count(), 810,
            "terminal reverse offset includes full unloading");
  expect_eq(terminal_reverse.duration.count(), 250,
            "terminal reverse duration includes full unloading");

  auto linehaul_source = std::make_shared<TransportCenter>("linehaul_source");
  auto linehaul_target = std::make_shared<TransportCenter>("linehaul_target");
  linehaul_source->set_latency<MovementType::CARTING, ProcessType::OUTBOUND>(
      DURATION{500});
  linehaul_target->set_latency<MovementType::CARTING, ProcessType::INBOUND>(
      DURATION{500});
  linehaul_source->set_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>(
      DURATION{10});
  linehaul_target->set_latency<MovementType::LINEHAUL, ProcessType::INBOUND>(
      DURATION{15});
  TransportEdge linehaul("linehaul", "route", DURATION{600}, DURATION{45},
                         DURATION{20}, DURATION{30}, VehicleType::SURFACE,
                         MovementType::LINEHAUL, false, day_mask(4));
  linehaul.update(linehaul_source, linehaul_target);
  const auto linehaul_forward = linehaul.weight<PathTraversalMode::FORWARD>();
  expect_eq(linehaul_forward.schedule_offset.count(), 580,
            "linehaul uses linehaul outbound latency");
  expect_eq(linehaul_forward.duration.count(), 95,
            "linehaul uses linehaul inbound latency");

  TransportEdge zero_duration("zero", "route", DURATION{600}, DURATION{0},
                              DURATION{0}, DURATION{0}, VehicleType::SURFACE,
                              MovementType::CARTING, false, day_mask(5));
  zero_duration.update(source, target);
  expect_eq(zero_duration.weight<PathTraversalMode::FORWARD>().duration.count(),
            50, "zero transit still includes center latencies");

  TransportEdge transient("transient", "transient");
  expect_eq(transient.weight<PathTraversalMode::FORWARD>().unreachable, true,
            "transient forward edge is unreachable");
  expect_eq(transient.weight<PathTraversalMode::REVERSE>().unreachable, true,
            "transient reverse edge is unreachable");
}

} // namespace

auto main() -> int {
  test_route_days_of_week_parsing();
  test_forward_schedule_traversal();
  test_reverse_schedule_traversal();
  test_transport_edge_weights();
  return EXIT_SUCCESS;
}
