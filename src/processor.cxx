#include "processor.hxx"
#include "date_utils.hxx"

#include "date_utils.hxx"
#include "processor.hxx"

template<>
nlohmann::json
parse_path<PathTraversalMode::FORWARD>(const std::shared_ptr<Segment> start)
{
  nlohmann::json response = {};

  if (start != nullptr) {
    auto current = start;

    while (current->next != nullptr) {
      nlohmann::json entry = {
        { "code", current->node->m_code },
        { "arrival", date::format("%D %T", current->distance) },
        { "arrival_ts", current->distance.time_since_epoch().count() },
        { "route",
          current->outbound->m_code.substr(
            0, current->outbound->m_code.find('.')) },
        { "departure",
          date::format(
            "%D %T",
            get_departure(current->distance, current->outbound->m_departure)) },
        { "departure_ts",
          get_departure(current->distance, current->outbound->m_departure)
            .time_since_epoch()
            .count() },
      };
      response.push_back(entry);
      current = current->next;
    }
    nlohmann::json entry = {
      { "code", current->node->m_code },
      { "arrival", date::format("%D %T", current->distance) },
      { "arrival_ts", current->distance.time_since_epoch().count() }
    };
    response.push_back(entry);
  }
  return response;
}

template<>
nlohmann::json
parse_path<PathTraversalMode::REVERSE>(const std::shared_ptr<Segment> start)
{
  nlohmann::json response = {};

  if (start != nullptr) {
    auto current = start;
    auto arrival = current->distance;

    while (current->next != nullptr) {
      nlohmann::json entry = {
        { "code", current->node->m_code },
        { "arrival", date::format("%D %T", arrival) },
        { "arrival_ts", arrival.time_since_epoch().count() },
        { "route",
          current->outbound->m_code.substr(
            0, current->outbound->m_code.find('.')) },
        { "departure", date::format("%D %T", current->distance) },
        { "departure_ts", current->distance.time_since_epoch().count() }
      };
      response.push_back(entry);
      arrival = current->distance + current->outbound->m_duration;
      current = current->next;
    }
    nlohmann::json entry{ { "code", current->node->m_code },
                          { "arrival", date::format("%D %T", arrival) },
                          { "arrival_ts",
                            arrival.time_since_epoch().count() } };
    response.push_back(entry);
  }
  return response;
}
