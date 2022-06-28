#ifndef PRODUCER_FAAS
#define PRODUCER_FAAS

#include "producer_api.hxx"

class FaaSApiProducer : public ApiProducer {
private:
  std::string mLastQueried;

  void set_params() override;

  auto parse_response(const json_t &) -> std::vector<json_t> override;

public:
  FaaSApiProducer(cpr::Url, std::string, std::string, queue_t *, size_t);
};

#endif
