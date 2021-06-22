#include "processor.hxx"
#include "date_utils.hxx"

#include "date_utils.hxx"
#include "processor.hxx"

template<>
nlohmann::json
parse_path<PathTraversalMode::FORWARD>(const Segment* start)
{
  nlohmann::json response = {};

  if (start != nullptr) {
    auto current = start;

    while (current->next != nullptr) {
      nlohmann::json entry = {
        { "code", current->node->code },
        { "arrival", date::format("%D %T", current->distance) },
        { "route",
          current->outbound->code.substr(0,
                                         current->outbound->code.find('.')) },
        { "departure",
          date::format(
            "%D %T",
            get_departure(current->distance, current->outbound->departure)) }
      };
      response.push_back(entry);
      current = current->next;
    }
    nlohmann::json entry = { { "code", current->node->code },
                             { "arrival",
                               date::format("%D %T", current->distance) } };
    response.push_back(entry);
  }
  return response;
}

template<>
nlohmann::json
parse_path<PathTraversalMode::REVERSE>(const Segment* start)
{
  nlohmann::json response = {};

  if (start != nullptr) {
    auto current = start;
    auto arrival = current->distance;

    while (current->next != nullptr) {
      nlohmann::json entry = { { "code", current->node->code },
                               { "arrival", date::format("%D %T", arrival) },
                               { "route",
                                 current->outbound->code.substr(
                                   0, current->outbound->code.find('.')) },
                               { "departure",
                                 date::format("%D %T", current->distance) } };
      response.push_back(entry);
      arrival = current->distance + current->outbound->duration;
    }
    nlohmann::json entry{ { "code", current->node->code },
                          { "arrival", date::format("%D %T", arrival) } };
    response.push_back(entry);
  }
  return response;
}
