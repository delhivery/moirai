#ifndef CLOTHO_KAFKA_CONSUMER_HXX
#define CLOTHO_KAFKA_CONSUMER_HXX

#include <cppkafka/cppkafka.h>
#include <cppkafka/topic_partition.h>
#include <memory>

namespace clotho {
namespace kafka {
class ClusterConfig;

class Consumer
{
private:
  std::shared_ptr<ClusterConfig> m_config;
  const std::string m_topic;
  const int32_t m_partition;
  const std::string m_consumer_group;
  std::vector<cppkafka::TopicPartition> m_topic_partitions;
  std::unique_ptr<cppkafka::Consumer> m_consumer;
  int64_t m_can_be_committed;
  int64_t m_last_committed;
  size_t m_max_pending_commits;
  uint64_t m_message_count;
  uint64_t m_message_bytes;
  bool m_eof;
  bool m_closed;

public:
  Consumer(std::shared_ptr<ClusterConfig>,
           std::string,
           int32_t,
           std::string,
           bool = true);

  ~Consumer();

  void close();

  std::unique_ptr<cppkafka::Message> consume(
    std::chrono::milliseconds = std::chrono::milliseconds{ 0 });

  inline bool eof() const;

  inline std::string topic() const;

  inline int32_t partition() const;

  void start(int64_t);

  void stop();

  int32_t commit(int64_t, bool = false);

  inline int64_t committed() const;

  int update_eof();

  bool consumer_group_exists(std::string, std::chrono::seconds) const;
};
}
}
#endif
