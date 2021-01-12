#include "streams/kafka.hxx"
#include "streams/kafka_utils.hxx"
#include <librdkafka/rdkafkacpp.h>
#include <stdexcept>
#include <string>
#ifdef __cpp_lib_format
#include <format>
#else
#include <fmt/core.h>
namespace std {
using fmt::format;
}
#endif

namespace moirai {
namespace streams {
void
KafkaConsumer::EventCallback::event_cb(RdKafka::Event& event)
{
  std::string info, error;
  switch (event.type()) {
    case RdKafka::Event::EVENT_ERROR:
      // log error
      error = std::format("{} {}", RdKafka::err2str(event.err()), event.str());
      break;
    case RdKafka::Event::EVENT_STATS:
      info = std::format("Stats: {}", event.str());
      break;
    case RdKafka::Event::EVENT_LOG:
      info = std::format("{}, {}", event.fac(), event.str());
      break;
    default:
      info = std::format("Event {} ({}): {}",
                         event.type(),
                         RdKafka::err2str(event.err()),
                         event.str());
      break;
  }
}

KafkaConsumer::KafkaConsumer(std::shared_ptr<ClusterConfig> config,
                             std::string topic,
                             int32_t partition,
                             std::string consumer_group,
                             bool check_cluster)
  : m_config(config)
  , m_topic(topic)
  , m_partition(partition)
  , m_consumer_group(consumer_group)
  , m_can_be_committed(-1)
  , m_last_committed(-1)
  , m_max_pending_commits(5000)
  , m_message_count(0)
  , m_message_bytes(0)
  , m_eof(false)
  , m_closed(false)
{
  std::string error;

  if (check_cluster) {
    if (config->get_cluster_metadata()->wait_for_topic_partition(
          topic, m_partition, config->get_cluster_state_timeout()) == false) {
      error = std::format(
        "Failed to wait for topic leaders, topic: {}:{}", topic, m_partition);
    }

    m_topic_partition.push_back(
      RdKafka::TopicPartition::create(m_topic, m_partition));

    std::unique_ptr<RdKafka::Conf> conf(
      RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));

    try {
      set_broker_config(conf.get(), m_config.get());

      set_config(conf.get(), "socket.nagle.disable", "true");
      set_config(
        conf.get(),
        "fetch.wait.max.ms",
        std::to_string(m_config->get_consumer_buffering_time().count()));
    } catch (std::invalid_argument& err) {
      error = std::format("Kafka consumer topic: {}:{}, bad config {}",
                          m_topic,
                          m_partition,
                          err.what());
    }
  }
}
}
}
