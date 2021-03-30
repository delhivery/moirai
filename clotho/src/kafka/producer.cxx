#include <chrono>
#include <clotho/kafka/cluster_config.hxx>
#include <clotho/kafka/producer.hxx>
#include <cppkafka/configuration.h>
#include <cppkafka/cppkafka.h>
#include <cppkafka/error.h>
#include <cppkafka/topic_configuration.h>
#include <spdlog/spdlog.h>
#include <string>

using namespace clotho::kafka;
inline std::string
Producer::topic() const
{
  return m_topic;
}

inline size_t
Producer::queue_size() const
{
  return m_closed ? 0 : m_rd_producer->get_out_queue_length();
}

inline void
Producer::poll()
{
  m_rd_producer->poll(std::chrono::milliseconds(m_rd_producer->get_timeout()));
}

inline void
Producer::poll(std::chrono::milliseconds timeout)
{
  m_rd_producer->poll(timeout);
}

inline bool
Producer::good() const
{
  if (m_error)
    return false;
  return true;
}

inline size_t
Producer::partition_count() const
{
  return m_partition_count;
}

inline int32_t
Producer::flush(std::chrono::milliseconds timeout)
{
  if (queue_size() == 0)
    return 0;
  try {
    m_rd_producer->flush(timeout);
  } catch (const cppkafka::Exception& exc) {
    return -1;
  }
  return 0;
}

Producer::Producer(std::shared_ptr<ClusterConfig> config, std::string topic)
  : m_topic(topic)
  , m_closed(false)
  , m_partition_count(0)
  , m_message_count(0)
  , m_message_bytes(0)
{
  if (config->get_cluster_metadata()->wait_for_topic_leaders(
        m_topic, config->get_cluster_state_timeout()) == false) {
    spdlog::critical("Failed to wait for topic leaders for topic {}", m_topic);
    exit(0);
  }

  cppkafka::Configuration gconf;
  cppkafka::TopicConfiguration tconf;

  try {
    // TODO set callback
    gconf.set_delivery_report_callback(
      [this](cppkafka::Producer& producer,
             const cppkafka::Message& msg) -> void {
        if (msg.get_error()) {
          m_error = msg.get_error();
        }
      });

    gconf.set("queue.buffering.max.ms",
              std::to_string(config->get_producer_buffering_time().count()));

    gconf.set("message.timeout.ms",
              std::to_string(config->get_producer_message_timeout().count()));

    gconf.set("socket.nagle.disable", "true");
    gconf.set("socket.max.fails", "1000000");
    gconf.set("message.send.max.retries", "1000000");
    gconf.set("log.connection.close", "false");

    gconf.set_error_callback([](cppkafka::KafkaHandleBase& handle,
                                int error,
                                const std::string& reason) -> void {
      spdlog::error("{} {}", error, reason);
    });

    gconf.set_throttle_callback(
      [](cppkafka::KafkaHandleBase& handle,
         const std::string& broker_name,
         int32_t broker_id,
         std::chrono::milliseconds throttle_time) -> void {
        spdlog::info("Throttled for {}ms", throttle_time.count());
      });

    gconf.set_log_callback([](cppkafka::KafkaHandleBase& handle,
                              int level,
                              const std::string& facility,
                              const std::string& message) -> void {
      spdlog::info("{}, {}", facility, message);
    });

    gconf.set_stats_callback(
      [](cppkafka::KafkaHandleBase& handle, const std::string& json) -> void {
        spdlog::info("Stats: {}", json);
      });

    gconf.set_background_event_callback(
      [](cppkafka::KafkaHandleBase& handle, cppkafka::Event event) -> void {});

    tconf.set_partitioner_callback([](const cppkafka::Topic&,
                                      const cppkafka::Buffer&,
                                      int32_t) -> int32_t { return 0; });
    gconf.set_default_topic_configuration(std::move(tconf));
  } catch (cppkafka::Exception& exc) {
    spdlog::critical("Topic: {}, bad config: ", m_topic, exc.what());
    exit(0);
  }

  try {
    m_rd_producer = std::make_unique<cppkafka::Producer>(gconf);
  } catch (const cppkafka::Exception& exc) {
    spdlog::critical(
      "Topic: {}, failed to create producer: {}", m_topic, exc.what());
  }

  // TODO Redundance m_rd_topic, use message builder
  // TODO Associate w/ producer
  // Original code m_rd_topic = RdKafka::Topic::create(producer.get(), m_topic,
  // tconf, error_string); Replace w/ messagebuilder
}

Producer::~Producer()
{
  if (not m_closed)
    close();
}

void
Producer::close()
{
  if (m_closed)
    return;

  m_closed = true;

  if (m_rd_producer and m_rd_producer->get_out_queue_length() > 0) {
    spdlog::info("Topic: {}, closing kafka producer - waiting for {} messages "
                 "to be written",
                 m_topic,
                 m_rd_producer->get_out_queue_length());
  }

  try {
    m_rd_producer->flush();
  } catch (const cppkafka::Exception& exc) {
    spdlog::info("Topic: {}, kafka producer flush did not finish in 2s: {}",
                 m_topic,
                 exc.what());
  }

  m_rd_producer.reset(nullptr);

  spdlog::info(
    "Topic: {}, kafka producer closed - produced {} messages ({} bytes)",
    m_topic,
    m_message_count,
    m_message_bytes);
}

int
Producer::produce(uint32_t partition_hash,
                  const std::string& key,
                  const std::string& value,
                  int64_t timestamp)
{
  m_rd_producer->produce(
    cppkafka::MessageBuilder(m_topic).key(key).payload(value).timestamp(
      std::chrono::milliseconds(timestamp)));
  return 0;
}
