#ifndef CLOTHO_CONSUMER_KAFKA_HXX
#define CLOTHO_CONSUMER_KAFKA_HXX
#include <clotho/consumer/consumer.hxx>
#include <cppkafka/configuration.h>
#include <string>

namespace ambasta {
class KafkaConsumer : public Consumer
{
private:
  std::string broker;
  cppkafka::Configuration* m_config;
  cppkafka::Consumer* consumer;
  std::string topic;

public:
  KafkaConsumer(SharedQueue, const uint16_t = 300);

  std::vector<std::string> read() const;
};
};
#endif
