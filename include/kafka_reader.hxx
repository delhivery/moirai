#ifndef MOIRAI_KAFKA_READER
#define MOIRAI_KAFKA_READER

#include "concurrentqueue.h"
#include "scan_reader.hxx"
#include "utils.hxx"
#include <Poco/Runnable.h>
#include <atomic>
#include <librdkafka/rdkafkacpp.h>
#include <string>
#include <vector>

class KafkaReader : public ScanReader
{
private:
  std::string mBrokerUrl;
  RdKafka::KafkaConsumer* mConsumerPtr;
  const uint16_t batchSize;
  const uint16_t timeout;
  static const std::string consumerGroup;
  std::string mTopic;
  moodycamel::ConcurrentQueue<std::string>* mNodeQueuePtr;
  moodycamel::ConcurrentQueue<std::string>* mEdgeQueuePtr;

public:
  KafkaReader(const std::string&,
              uint16_t,
              uint16_t,
              std::string,
              moodycamel::ConcurrentQueue<std::string>*,
              moodycamel::ConcurrentQueue<std::string>*,
              moodycamel::ConcurrentQueue<std::string>*);

  ~KafkaReader() override;

  auto consume_batch(size_t, int) -> std::vector<RdKafka::Message*>;

  void run() override;
};

#endif
