#ifndef MOIRAI_KAFKA_READER
#define MOIRAI_KAFKA_READER

#include "concurrentqueue.h"
#include "utils.hxx"
#include <Poco/Runnable.h>
#include <atomic>
#include <librdkafka/rdkafkacpp.h>
#include <string>
#include <vector>

class KafkaReader : public Poco::Runnable
{
private:
  std::string broker_url;
  RdKafka::Conf* config;
  RdKafka::KafkaConsumer* consumer;
  const uint16_t batch_size;
  const uint16_t timeout;
  static const std::string consumer_group;
  std::vector<std::string> topics;
  StringToStringMap topic_map;
  std::atomic<bool> running;
  moodycamel::ConcurrentQueue<std::string>* load_queue;

public:
  KafkaReader(const std::string&,
              const uint16_t,
              const uint16_t,
              const StringToStringMap&,
              moodycamel::ConcurrentQueue<std::string>*);

  ~KafkaReader();

  std::vector<RdKafka::Message*> consume_batch(RdKafka::KafkaConsumer*,
                                               size_t,
                                               int);

  virtual void run();
};

#endif
