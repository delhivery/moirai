#ifndef MOIRAI_KAFKA_READER
#define MOIRAI_KAFKA_READER

#include "producer.hxx"
#include <Poco/Logger.h>
#include <librdkafka/rdkafkacpp.h>

class KafkaReader : public Producer
{
private:
  static const std::string consumerGroup;

  // Poco::Logger& mLogger = Poco::Logger::get("scan-reader");
  RdKafka::KafkaConsumer* mKafkaConsumerPtr;

  const uint16_t mTimeout;

  auto fetch() -> nlohmann::json override;

public:
  KafkaReader(const std::string&, std::string, uint16_t, queue_t*, size_t);

  ~KafkaReader() override;
};

#endif
