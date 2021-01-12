#pragma once

#include <cstdint>
#include <librdkafka/rdkafkacpp.h>
#include <memory>
#include <vector>

namespace moirai {
namespace streams {
class ClusterConfig;

class KafkaConsumer
{
public:
  KafkaConsumer(std::shared_ptr<ClusterConfig>,
                std::string,
                int32_t,
                std::string,
                bool);

  ~KafkaConsumer();

  void close();

  std::unique_ptr<RdKafka::Message> consume(int = 0);

  inline bool eof() const;

  inline std::string topic() const;

  inline int32_t partition() const;

  void start(int64_t);

  void stop();

  int32_t commit(int64_t, bool = false);

  inline int64_t committed() const;

  int update_eof();

  bool consumer_group_exists(std::string, std::chrono::seconds) const;

private:
  class EventCallback : public RdKafka::EventCb
  {
  public:
    void event_cb(RdKafka::Event&);
  };

  std::shared_ptr<ClusterConfig> m_config;
  const std::string m_topic;
  const std::string m_consumer_group;
  const int32_t m_partition;
  std::vector<RdKafka::TopicPartition*> m_topic_partition;
  std::unique_ptr<RdKafka::KafkaConsumer> m_consumer;
  int64_t m_can_be_committed;
  int64_t m_last_committed;
  std::size_t m_max_pending_commits;
  uint64_t m_message_count;
  uint64_t m_message_bytes;
  bool m_eof;
  bool m_closed;
  EventCallback m_event_callaback;
};

}
}
