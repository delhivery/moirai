#include "producer_thanos.hxx"
#include "concepts"
#include <range/v3/range.hpp>

ThanosApiProducer::ThanosApiProducer(cpr::Url url, std::string token,
                                     queue_t *qPtr, size_t mBatchSize)
    : ApiProducer(url, token, qPtr, mBatchSize) {}

void ThanosApiProducer::set_params() {}

auto ThanosApiProducer::parse_response(const json_t &payload)
    -> std::vector<json_t> {
  auto data = payload[jptr_t{"/data"}];

  if (not data.empty() and not data.is_null()) {
    auto routes =
        data | std::views::transform([](const auto &routeData) {
          json_t routeMeta;
          routeMeta["code"] = routeData["route_schedule_uuid"];
          routeMeta["name"] = routeData["name"];
          routeMeta["type"] = routeData["route_type"];
          routeMeta["wDay"] =
              routeData["days_of_week"] |
              std::views::transform([](const auto &wDay) -> uint8_t {
                return static_cast<uint8_t>(wDay);
              });

          auto offset =
              static_cast<uint16_t>((double)routeMeta["reporting_time_ss"]);
          return routeData["halt_centers"] |
                 std::views::adjacent_transform(
                     [&routeMeta, &offset,
                      &wrkDay](const auto &sHalt, const auto &tHalt) -> json_t {
                       json_t retVal = routeMeta;
                       auto sRelArr =
                           static_cast<uint16_t>((double)sHalt["rel_eta_ss"]);
                       auto sRelDep =
                           static_cast<uint16_t>((double)sHalt["rel_etd_ss"]);
                       auto tRelArr =
                           static_cast<uint16_t>((double)tHalt["rel_eta_ss"]);
                       auto tRelDep =
                           static_cast<uint16_t>((double)tHalt["rel_etd_ss"]);
                       retVal["eDep"] = offset + sRelDep;
                       retVal["eOut"] = sRelDep - sRelArr;
                       retVal["eDur"] = tRelArr - sRelDep;
                       retval["eInb"] = tRelDep - tRelArr;
                       retval["eSrc"] = sHalt["center_code"];
                       retVal["eTar"] = tHalt["center_code"];
                       return retVal;
                     });
        });
    stop(true);
    return std::views::join(routes | std::views::take(1),
                            routes | std::views::drop(1) |
                                std::views::take(routes.size() - 2) |
                                std::views::transform([](const auto &subRoute) {
                                  route["eInb"] = (uint16_t)route["eInb"] / 2;
                                  route["eOut"] = (uint16_t)route["eOut"] / 2;
                                  return route;
                                }),
                            routes | std::views::drop(routes.size() - 1));
  } else {
    stop(true);
  }
  return {};
}
