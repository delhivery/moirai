#ifndef MOIRAI_KAFKA_READER
#define MOIRAI_KAFKA_READER

#include "producer.hxx"
#include <librdkafka/rdkafkacpp.h>

class MSKLoadProducer : public Producer {
private:
  static const std::string CONSUMER_GROUP;

  RdKafka::KafkaConsumer *mKafkaConsumerPtr;

  const uint16_t mTimeout;

  auto fetch() -> std::vector<json_t> override;

  auto logger() const -> Poco::Logger & override;

public:
  MSKLoadProducer(std::string_view, std::string_view, uint16_t, queue_t *,
                  size_t);

  ~MSKLoadProducer() override;
};

#endif
