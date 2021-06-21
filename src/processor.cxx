#include "processor.hxx"
#include "date_utils.hxx"

nlohmann::json
parse_path(const Segment* path)
{
  nlohmann::json response = {};

  if (path != nullptr) {
    auto segment = path;

    while (segment->next != nullptr) {
      nlohmann::json entry = {
        { "code", segment->node->code },
        { "arrival", date::format("%D %T", segment->distance) },
        { "route",
          segment->outbound->code.substr(0,
                                         segment->outbound->code.find('.')) },
        { "departure",
          date::format(
            "%D %T",
            get_departure(segment->distance, segment->outbound->departure)) }
      };
      response.push_back(entry);
      segment = segment->next;
    }
    nlohmann::json entry = { { "code", segment->node->code },
                             { "arrival",
                               date::format("%D %T", segment->distance) } };
    response.push_back(entry);
  }

  std::reverse(response.begin(), response.end());
  return response;
}
