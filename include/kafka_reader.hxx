#ifndef MOIRAI_KAFKA_READER
#define MOIRAI_KAFKA_READER

#include "scan_reader.hxx"
#include <Poco/Logger.h>
#include <librdkafka/rdkafkacpp.h>

class KafkaReader : public ScanReader
{
private:
  Poco::Logger& mLogger = Poco::Logger::get("scan-reader");
  std::string mBrokerUrl;
  RdKafka::KafkaConsumer* mConsumerPtr;
  const uint16_t batchSize;
  const uint16_t timeout;
  static const std::string consumerGroup;
  std::string mTopic;

public:
  KafkaReader(const std::string&,
              uint16_t,
              uint16_t,
              std::string,
              moodycamel::ConcurrentQueue<std::string>*);

  ~KafkaReader() override;

  auto consume_batch(size_t, int) -> std::vector<RdKafka::Message*>;

  void run() override;
};

#endif
