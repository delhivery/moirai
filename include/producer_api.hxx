#ifndef MOIRAI_PRODUCER_API
#define MOIRAI_PRODUCER_API

#include "producer.hxx"
#include <cpr/cprtypes.h>
#include <cpr/session.h>

class ApiProducer : public Producer {
private:
  auto fetch() -> std::vector<json_t> override;

protected:
  cpr::Session mSession;

  virtual void set_params() = 0;

  virtual auto parse_response(const json_t &) -> std::vector<json_t> = 0;

public:
  ApiProducer(cpr::Url, queue_t *, size_t);

  ApiProducer(cpr::Url, std::string, queue_t *, size_t);

  ApiProducer(cpr::Url, std::string, std::string, queue_t *, size_t);
};
#endif
