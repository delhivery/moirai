#include "producer_faas.hxx"

FaaSApiProducer::FaaSApiProducer(cpr::Url url, std::string user,
                                 std::string pass, queue_t *ptr,
                                 size_t batchSize)
    : ApiProducer(url, user, pass, ptr, batchSize), mLastQueried("") {}

void FaaSApiProducer::set_params() {
  json_t query = {{"query",
                   {"term", {"active", {"value", true}}},
                   "_source",
                   {
                       "name",
                       "facility_code",
                       "property_id",
                       "facility_attributes.OutboundProcessingTime",
                       "facility_attributes.CenterArrivalCutoff",
                   },
                   "size",
                   mBatchSize,
                   "sort",
                   {"facility_code:asc"}}};
  if (mLastQueried.empty()) {
    mSession.SetBody(cpr::Body{query.dump()});
  } else {
    query["search_after"] = mLastQueried;
  }
}

auto FaaSApiProducer::parse_response(const json_t &response)
    -> std::vector<json_t> {
  auto data = response[jptr_t{"/hits/hits"}];

  if (not data.empty() and not data.is_null()) {
    auto facilities =
        data | std::views::transform([](const auto &facilityJson) {
          json_t facility;
          facility["code"] = facilityJson[jptr_t{"/_source/facility_code"}];
          facility["name"] = facilityJson[jptr_t{"/_source/name"}];
          facility["group"] = facilityJson[jptr_t{"/_source/property_id"}];
          facility["cutoff"] = facilityJson[jptr_t{
              "/_source/facility_attributes/CenterArrivalCutoff"}];
          facility["outProces"] = facilityJson[jptr_t{
              "/_source/facility_attributes/OutboundProcessingTime"}];
          facility["inProcess"] = 0;
          return facility;
        });
    std::vector<json_t> retVal{facilities.begin(), facilities.end()};
    mLastQueried = retVal.back()["code"];
    return retVal;
  } else {
    stop(true);
  }
  return {};
}
