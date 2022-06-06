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
  std::string broker_url;
  RdKafka::KafkaConsumer* consumer;
  const uint16_t batch_size;
  const uint16_t timeout;
  static const std::string consumer_group;
  std::vector<std::string> topics;
  StringToStringMap topic_map;
  moodycamel::ConcurrentQueue<std::string>* node_queue;
  moodycamel::ConcurrentQueue<std::string>* edge_queue;

public:
  KafkaReader(const std::string&,
              const uint16_t,
              const uint16_t,
              const StringToStringMap&,
              moodycamel::ConcurrentQueue<std::string>*,
              moodycamel::ConcurrentQueue<std::string>*,
              moodycamel::ConcurrentQueue<std::string>*);

  ~KafkaReader();

  std::vector<RdKafka::Message*> consume_batch(size_t, int);

  virtual void run();
};

#endif
