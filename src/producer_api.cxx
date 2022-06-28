#include "producer_api.hxx"
#include <cpr/auth.h>

ApiProducer::ApiProducer(cpr::Url url, queue_t *qPtr, size_t batchSize)
    : Producer(qPtr, batchSize) {
  mSession.SetUrl(url);
  mSession.SetHeader(
      {{"Content-Type", "application/json"}, {"Accept", "application/json"}});
}

ApiProducer::ApiProducer(cpr::Url url, std::string token, queue_t *qPtr,
                         size_t batchSize)
    : ApiProducer(url, qPtr, batchSize) {
  mSession.SetBearer(cpr::Bearer{token});
}

ApiProducer::ApiProducer(cpr::Url url, std::string user, std::string pass,
                         queue_t *qPtr, size_t batchSize)
    : ApiProducer(url, qPtr, batchSize) {
  mSession.SetAuth({user, pass, cpr::AuthMode::BASIC});
}

auto ApiProducer::fetch() -> std::vector<json_t> {
  set_params();

  auto response = mSession.Get();

  if (response.status_code == 200) {
    auto response_json = json_t::parse(response.text);
    return parse_response(response_json);
  } else {
    stop(true);
  }
  return {};
}
