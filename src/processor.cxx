#include "processor.hxx"
#include "date_utils.hxx"

#include "date_utils.hxx"
#include "processor.hxx"
#include <algorithm>
#include <fmt/chrono.h>

nlohmann::json
parse_path(const std::vector<Segment>& route)
{
  nlohmann::json data = nlohmann::json::array();
  std::transform(
    route.begin(),
    route.end(),
    std::back_inserter(data),
    [](const Segment& segment) {
      std::string departure = fmt::format(
        "{:%D %T}", segment.m_outbound->departure(segment.m_distance));
      return nlohmann::json{ { "code", segment.m_node->m_code },
                             { "arrival",
                               fmt::format("{:%D %T}", segment.m_distance) },
                             { "route", segment.m_outbound->m_code },
                             { "departure", departure } };
    });
  return data;
}
