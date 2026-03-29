#include "processor.hxx"
#include "date_utils.hxx"
#include <nlohmann/json.hpp>

template <>
auto parse_path<PathTraversalMode::FORWARD>(
    const std::shared_ptr<Segment> &start) -> nlohmann::json {
  nlohmann::json response = {};

  if (start != nullptr) {
    auto current = start;

    while (current->next != nullptr) {
      nlohmann::json entry = {
          {"code", current->node->code},
          {"arrival", format_clock(current->distance)},
          {"arrival_ts", current->distance.time_since_epoch().count()},
          {"route", current->outbound->code.substr(
                        0, current->outbound->code.find('.'))},
          {"departure", format_clock(get_departure(
                            current->distance, current->outbound->departure))},
          {"departure_ts",
           get_departure(current->distance, current->outbound->departure)
               .time_since_epoch()
               .count()},
      };
      response.push_back(entry);
      current = current->next;
    }
    nlohmann::json entry = {
        {"code", current->node->code},
        {"arrival", format_clock(current->distance)},
        {"arrival_ts", current->distance.time_since_epoch().count()}};
    response.push_back(entry);
  }
  return response;
}

template <>
auto parse_path<PathTraversalMode::REVERSE>(
    const std::shared_ptr<Segment> &start) -> nlohmann::json {
  nlohmann::json response = {};

  if (start != nullptr) {
    auto current = start;
    auto arrival = current->distance;

    while (current->next != nullptr) {
      nlohmann::json entry = {
          {"code", current->node->code},
          {"arrival", format_clock(arrival)},
          {"arrival_ts", arrival.time_since_epoch().count()},
          {"route", current->outbound->code.substr(
                        0, current->outbound->code.find('.'))},
          {"departure", format_clock(current->distance)},
          {"departure_ts", current->distance.time_since_epoch().count()}};
      response.push_back(entry);
      arrival = current->distance + current->outbound->duration;
      current = current->next;
    }
    nlohmann::json entry{{"code", current->node->code},
                         {"arrival", format_clock(arrival)},
                         {"arrival_ts", arrival.time_since_epoch().count()}};
    response.push_back(entry);
  }
  return response;
}
