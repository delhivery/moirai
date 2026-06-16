import std;

import moirai.app;
import moirai.date_utils;
import moirai.http;
import moirai.json_utils;
import moirai.solver_wrapper;
import moirai.route_schedule;
import moirai.solver;
import moirai.transportation;
import moirai.utils;

auto main() -> int {
  const DURATION one_hour{60};
  const auto monday = iso_to_date("2026-06-08 00:00:00");
  const TransportEdge edge("module-edge", "Module Edge");

  moirai::JsonParser parser;
  std::string route_json = R"({"days_of_week":[1,2,3]})";
  auto parsed = moirai::parse_json(parser, route_json);
  if (!parsed.has_value()) {
    return 1;
  }
  const moirai::Json& route = *parsed;
  Solver solver;
  moirai::Application::instance().logger().set_level("error");

  if (one_hour.count() != 60 || monday.time_since_epoch().count() == 0 ||
      edge.code != "module-edge" || parse_route_days_of_week(route) == 0 ||
      solver.show() != "Graph<0, 0>") {
    return 1;
  }

  std::println("module smoke ok");
  return 0;
}
