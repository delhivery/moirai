module moirai.processor;

import std;
import moirai.date_utils;
import moirai.search_document;
import moirai.solver;
import moirai.transportation;

template <>
void parse_path_into<PathTraversalMode::FORWARD>(
    const Path& path, std::vector<SearchPathLocation>& response) {
  response.clear();
  response.reserve(path.steps.size());

  for (const auto& step : path.steps) {
    if (step.node == nullptr) {
      continue;
    }
    if (step.outbound != nullptr) {
      const auto departure =
        get_departure(step.distance, step.outbound->departure);
      SearchPathLocation entry = {
        .code = step.node->code,
        .facility_name = step.node->name,
        .arrival = format_clock(step.distance),
        .arrival_ts = step.distance.time_since_epoch().count() * 60,
        .route = step.outbound->route_prefix,
        .route_name = step.outbound->name,
        .departure = format_clock(departure),
        .departure_ts = departure.time_since_epoch().count() * 60,
        .has_departure = true,
      };
      response.push_back(std::move(entry));
    } else {
      SearchPathLocation terminal;
      terminal.code = step.node->code;
      terminal.facility_name = step.node->name;
      terminal.arrival = format_clock(step.distance);
      terminal.arrival_ts = step.distance.time_since_epoch().count() * 60;
      response.push_back(std::move(terminal));
    }
  }
}

template <>
void parse_path_into<PathTraversalMode::REVERSE>(
    const Path& path, std::vector<SearchPathLocation>& response) {
  response.clear();
  response.reserve(path.steps.size());

  if (!path.steps.empty()) {
    auto arrival = path.steps.front().distance;
    for (const auto& step : path.steps) {
      if (step.node == nullptr) {
        continue;
      }
      if (step.outbound == nullptr) {
        SearchPathLocation terminal;
        terminal.code = step.node->code;
        terminal.facility_name = step.node->name;
        terminal.arrival = format_clock(arrival);
        terminal.arrival_ts = arrival.time_since_epoch().count() * 60;
        response.push_back(std::move(terminal));
        continue;
      }

      SearchPathLocation entry = {
        .code = step.node->code,
        .facility_name = step.node->name,
        .arrival = format_clock(arrival),
        .arrival_ts = arrival.time_since_epoch().count() * 60,
        .route = step.outbound->route_prefix,
        .route_name = step.outbound->name,
        .departure = format_clock(step.distance),
        .departure_ts = step.distance.time_since_epoch().count() * 60,
        .has_departure = true,
      };
      response.push_back(std::move(entry));
      arrival = step.distance + step.outbound->duration;
    }
  }
}

template <>
auto parse_path<PathTraversalMode::FORWARD>(
    const Path& path) -> std::vector<SearchPathLocation> {
  std::vector<SearchPathLocation> response;
  parse_path_into<PathTraversalMode::FORWARD>(path, response);
  return response;
}

template <>
auto parse_path<PathTraversalMode::REVERSE>(
    const Path& path) -> std::vector<SearchPathLocation> {
  std::vector<SearchPathLocation> response;
  parse_path_into<PathTraversalMode::REVERSE>(path, response);
  return response;
}
