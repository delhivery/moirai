#include <clotho/kafka/cluster_config.hxx>
#include <clotho/kafka/consumer.hxx>
#include <cppkafka/consumer.h>
#include <cppkafka/topic_configuration.h>
#include <cppkafka/topic_partition.h>
#include <spdlog/spdlog.h>

using namespace std::chrono_literals;
using namespace clotho::kafka;

Consumer::Consumer(std::shared_ptr<ClusterConfig> config,
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
  if (check_cluster) {
    if (config->get_cluster_metadata()->wait_for_topic_partition(
          m_topic, m_partition, config->get_cluster_state_timeout()) == false) {
      spdlog::critical(
        "Failed to wait for topic leaders, topic: {}:{}", m_topic, m_partition);
    }
  }
  m_topic_partitions.push_back(cppkafka::TopicPartition(m_topic, m_partition));

  cppkafka::Configuration g_conf{};
  cppkafka::TopicConfiguration t_conf{};

  try {
    g_conf.set("socket.nagle.disable", "true");
    g_conf.set("fetch.wait.max.ms",
               std::to_string(m_config->get_consumer_buffering_time().count()));
    g_conf.set("enable.auto.commit", "false");
    g_conf.set("enable.auto.offset.store", "false");
    g_conf.set("group.id", m_consumer_group);
    g_conf.set("enable.partition.eof", "true");
    g_conf.set("log.connection.close", "false");
    g_conf.set("max.poll.interval.ms", "86400000");
    // Add all event callbacks
    g_conf.set_error_callback(
      [](cppkafka::KafkaHandleBase&, int, const std::string&) -> void {});

    t_conf.set("auto.offset.reset", "earliest");
    g_conf.set_default_topic_configuration(t_conf);
  } catch (const cppkafka::Exception& exc) {
    spdlog::critical(
      "Kafka consumer topic {}:{}, bad config", m_topic, m_partition);
    exit(0);
  }

  try {
    m_consumer = std::make_unique<cppkafka::Consumer>(g_conf);
  } catch (const cppkafka::Exception& exc) {
    spdlog::critical(
      "Kafka consumer topic: {}:{}, failed to create consumer. Reason: {}",
      m_topic,
      m_partition,
      exc.what());
    exit(0);
  }
  spdlog::info("Kafka consumer topic: {}:{}, created", m_topic, m_partition);
}

Consumer::~Consumer()
{
  if (not m_closed)
    close();
  spdlog::info("Consumer deleted");
}

void
Consumer::close()
{
  if (m_closed)
    return;
  m_consumer.reset(nullptr);
}

void
Consumer::start(int64_t offset)
{
  // TODO Define offset enums
  switch (offset) {
    case cppkafka::TopicPartition::OFFSET_STORED:
      if (consumer_group_exists(m_consumer_group, 5s)) {
        spdlog::info("Kafka consumer. Start topic: {}:{}, consumer group {}, "
                     "starting from OFFSET_STORED",
                     m_topic,
                     m_partition,
                     m_consumer_group);
      } else {
        spdlog::info("Kafka consumer. Start topic: {}:{}, consumer group {} "
                     "missing, starting from OFFSET_BEGINNING");
        offset = cppkafka::TopicPartition::OFFSET_BEGINNING;
      }
      break;
    case cppkafka::TopicPartition::OFFSET_BEGINNING:
      spdlog::info("Kafka consumer. Start topic: {}:{}, consumer group {}, "
                   "starting from OFFSET_BEGINNING",
                   m_topic,
                   m_partition,
                   m_consumer_group);
      break;
    case cppkafka::TopicPartition::OFFSET_END:
      spdlog::info("Kafka consumer. Start topic: {}:{}, consumer group {}, "
                   "starting from OFFSET_END",
                   m_topic,
                   m_partition,
                   m_consumer_group);
      break;
    default:
      spdlog::info("Kafka consumer. Start topic: {}:{}, consumer group {}, "
                   "starting from fixed offset {}",
                   m_topic,
                   m_partition,
                   m_consumer_group,
                   offset);
  }

  try {
    m_topic_partitions[0].set_offset(offset);
  } catch (const cppkafka::Exception& exc) {
    spdlog::critical(
      "Kafka consumer. Topic: {}:{}, failed to subscribe, reason {}",
      m_topic,
      m_partition,
      exc.what());
    exit(0);
  }

  update_eof();
}

void
Consumer::stop()
{
  if (m_consumer) {
    try {
      m_consumer->unassign();
    } catch (const cppkafka::Exception& exc) {
      spdlog::critical(
        "Kafka consumer. Stop topic: {}:{}, failed to stop. Reason: {}",
        m_topic,
        m_partition,
        exc.what());
      exit(0);
    }
  }
}

int
Consumer::update_eof()
{
  int64_t low = 0;
  int64_t high = 0;

  try {
    std::tie(low, high) =
      m_consumer->query_offsets(cppkafka::TopicPartition(m_topic, m_partition));
    if (low == high) {
      m_eof = true;
      spdlog::info("Kafka consumer. Topic: {}:{} empty. EoF @ {}",
                   m_topic,
                   m_partition,
                   high);
    } else {
      m_consumer->get_offsets_position(m_topic_partitions);
      auto cursor = m_topic_partitions[0].get_offset();
      m_eof = (cursor + 1 == high);
      spdlog::info("Kafka consumer. Topic: {}:{}, cursor: {}, eof at {}",
                   m_topic,
                   m_partition,
                   cursor,
                   high);
    }
  } catch (const cppkafka::Exception& exc) {

    spdlog::info("Kafka consumer. Topic: {}:{}, "
                 "consumer.query_watermark_offsets failed. Reason: {}",
                 m_topic,
                 m_partition,
                 exc.what());
    return -1;
  }
  return 0;
}

std::unique_ptr<cppkafka::Message>
Consumer::consume(std::chrono::milliseconds timeout)
{
  if (m_closed or m_consumer == nullptr) {
    spdlog::error("Topic: {}:{}, consume failed. Closed", m_topic, m_partition);
    return nullptr;
  }

  try {
    cppkafka::Message message = m_consumer->poll(timeout);
    m_eof = false;
    m_message_count++;
    m_message_bytes +=
      message.get_payload().get_size() + message.get_key().get_size();
    return std::make_unique<cppkafka::Message>(std::move(message));
  } catch (cppkafka::Exception& exc) {
  }
  return nullptr;
}
