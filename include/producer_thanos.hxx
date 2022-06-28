#ifndef PRODUCER_THANOS
#define PRODUCER_THANOS

#include "producer_api.hxx"

class ThanosApiProducer : public ApiProducer {
private:
  void set_params() override;

  auto parse_response(const json_t &) -> std::vector<json_t> override;

public:
  ThanosApiProducer(cpr::Url, std::string, queue_t *, size_t);
};

#endif
