module;

#include "blocking_queue.hxx"
#include <librdkafka/rdkafkacpp.h>

module moirai.kafka_reader;

import std;
import moirai.app;
import moirai.scan_reader;
import moirai.utils;

namespace {

constexpr auto IMMEDIATE_POLL_TIMEOUT = std::chrono::milliseconds{0};
const auto SASL_PROPERTIES = std::to_array<std::string_view>(
    {"sasl.mechanism", "sasl.mechanisms", "sasl.username", "sasl.password"});

auto has_sasl_configuration(
    const std::unordered_map<std::string, std::string> &properties) -> bool {
  return std::ranges::any_of(SASL_PROPERTIES,
                             [&properties](std::string_view key) -> bool {
                               return properties.contains(std::string(key));
                             });
}

} // namespace

void KafkaReader::ConsumerDeleter::operator()(
    RdKafka::KafkaConsumer *consumer) const {
  if (consumer != nullptr) {
    consumer->close();
    delete consumer;
  }
}

KafkaReader::KafkaReader(
    const std::string &broker_url, size_t batch_size,
    std::chrono::milliseconds timeout, TopicMap topic_map,
    std::unordered_map<std::string, std::string> properties, QueueSet queues)
    : ScanReader(queues.load), m_broker_url(broker_url),
      m_batch_size(batch_size), m_timeout(timeout),
      m_topic_map(std::move(topic_map)), m_properties(std::move(properties)),
      m_node_queue(queues.node), m_edge_queue(queues.edge) {
  auto &app = moirai::Application::instance();
  app.logger().debug("Configuring kafka reader");
  using ConfigPtr = std::unique_ptr<RdKafka::Conf, void (*)(RdKafka::Conf *)>;
  ConfigPtr config(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL),
                   [](RdKafka::Conf *conf) -> void { delete conf; });

  std::string error_string;
  const auto set_config = [&app, &config,
                           &error_string](const std::string &key,
                                          const std::string &value) -> void {
    if (config->set(key, value, error_string) != RdKafka::Conf::CONF_OK) {
      app.logger().error("Error setting kafka property {}: {}", key,
                         error_string);
      throw std::runtime_error(error_string);
    }
  };

  set_config("enable.partition.eof", "false");
  set_config("group.id", std::string(CONSUMER_GROUP));
  set_config("bootstrap.servers", broker_url);

  if (has_sasl_configuration(m_properties) &&
      !m_properties.contains("security.protocol")) {
    app.logger().information(
        "Detected SASL kafka properties; defaulting security.protocol to "
        "SASL_SSL");
    set_config("security.protocol", "SASL_SSL");
  }

  if (!m_properties.contains("auto.offset.reset")) {
    set_config("auto.offset.reset", "latest");
  }
  if (!m_properties.contains("queued.min.messages")) {
    set_config("queued.min.messages", "5000");
  }
  if (!m_properties.contains("queued.max.messages.kbytes")) {
    set_config("queued.max.messages.kbytes", "65536");
  }

  for (const auto &[key, value] : m_properties) {
    set_config(key, value);
  }

  m_consumer.reset(RdKafka::KafkaConsumer::create(config.get(), error_string));

  if (!m_consumer) {
    app.logger().error(
        std::format("Error creating consumer. {}", error_string));
    throw std::runtime_error(error_string);
  }

  m_topics.push_back(m_topic_map.topic_for("load"));
  if (m_node_queue != nullptr && m_topic_map.contains_role("node")) {
    m_topics.push_back(m_topic_map.topic_for("node"));
  }
  if (m_edge_queue != nullptr && m_topic_map.contains_role("edge")) {
    m_topics.push_back(m_topic_map.topic_for("edge"));
  }

  if (RdKafka::ErrorCode error_code = m_consumer->subscribe(m_topics);
      error_code) {
    app.logger().error(std::format("Error subscribing to {} topics: {}",
                                   m_topics.size(),
                                   RdKafka::err2str(error_code)));
    throw std::runtime_error(RdKafka::err2str(error_code));
  }
}

KafkaReader::~KafkaReader() = default;

auto KafkaReader::consume_message(std::chrono::milliseconds timeout)
    -> std::unique_ptr<RdKafka::Message> {
  auto &app = moirai::Application::instance();
  const auto timeout_ms = static_cast<int>(
      std::clamp<int64_t>(timeout.count(), 0, std::numeric_limits<int>::max()));
  auto message =
      std::unique_ptr<RdKafka::Message>(m_consumer->consume(timeout_ms));
  if (!message) {
    app.logger().error("Kafka consumer returned a null message");
    return nullptr;
  }

  return message;
}

auto KafkaReader::dispatch_message(const RdKafka::Message &message,
                                   const std::stop_token &stop_token) -> bool {
  auto &app = moirai::Application::instance();
  app.logger().debug("Message in {} [{}] at offset {}", message.topic_name(),
                     message.partition(), message.offset());

  const auto *payload = static_cast<const char *>(message.payload());
  const std::string data =
      payload == nullptr ? std::string{} : std::string(payload, message.len());
  if (!m_topic_map.contains_topic(message.topic_name())) {
    app.logger().error("Unsupported topic: {}", message.topic_name());
    return true;
  }

  const std::string &topic_name = m_topic_map.role_for(message.topic_name());
  if (topic_name == "load") {
    return m_load_queue.wait_enqueue(data, stop_token);
  }

  if (topic_name == "edge") {
    return m_edge_queue == nullptr ||
           m_edge_queue->wait_enqueue(data, stop_token);
  }

  if (topic_name == "node") {
    return m_node_queue == nullptr ||
           m_node_queue->wait_enqueue(data, stop_token);
  }

  app.logger().error("Unsupported topic: {}", message.topic_name());
  return true;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void KafkaReader::run(std::stop_token stop_token) {
  auto &app = moirai::Application::instance();

  while (!stop_token.stop_requested()) {
    auto message = consume_message(m_timeout);
    if (!message) {
      continue;
    }

    size_t dispatched = 0;
    while (message != nullptr && !stop_token.stop_requested()) {
      switch (message->err()) {
      case RdKafka::ERR__TIMED_OUT:
      case RdKafka::ERR__PARTITION_EOF:
        message.reset();
        break;
      case RdKafka::ERR_NO_ERROR:
        ++dispatched;
        if (!dispatch_message(*message, stop_token)) {
          return;
        }
        message = dispatched < m_batch_size
                      ? consume_message(IMMEDIATE_POLL_TIMEOUT)
                      : nullptr;
        break;
      default:
        app.logger().error("%% Consumer error: {}", message->errstr());
        message.reset();
        break;
      }
    }

    if (dispatched > 0) {
      app.logger().debug("Dispatched {} kafka messages", dispatched);
    }
  }
}
