#include "kafka_reader.hxx"
#include "date_utils.hxx"
#include <Poco/Util/ServerApplication.h>

#ifdef __cpp_lib_format
#include <format>
#else
#include <fmt/core.h>
namespace std {
using fmt::format;
};
#endif

const std::string KafkaReader::consumer_group = "MOIRAI";

KafkaReader::KafkaReader(const std::string& broker_url,
                         const uint16_t batch_size,
                         const uint16_t timeout,
                         const StringToStringMap& topic_map,
                         moodycamel::ConcurrentQueue<std::string>* load_queue)
  : Poco::Runnable()
  , broker_url(broker_url)
  , batch_size(batch_size)
  , timeout(timeout)
  , topic_map(topic_map)
  , load_queue(load_queue)
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  app.logger().debug("Configuring kafka reader");
  config = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

  std::string error_string;

  if (config->set("enable.partition.eof", "false", error_string) !=
      RdKafka::Conf::CONF_OK) {
    app.logger().error(
      std::format("Error enabling partition eof: {}", error_string));
    throw Poco::ApplicationException(error_string);
  }

  if (config->set("group.id", consumer_group, error_string) !=
      RdKafka::Conf ::CONF_OK) {
    app.logger().error(std::format("Error setting group id: {}", error_string));
    throw Poco::ApplicationException(error_string);
  }

  if (config->set("bootstrap.servers", broker_url, error_string) !=
      RdKafka::Conf::CONF_OK) {
    app.logger().error(
      std::format("Error bootstrapping servers. {}", error_string));
    throw Poco::ApplicationException(error_string);
  }

  if (consumer = RdKafka::KafkaConsumer::create(config, error_string);
      !consumer) {
    app.logger().error(
      std::format("Error creating consumer. {}", error_string));
    throw Poco::ApplicationException(error_string);
  }

  delete config;

  for (auto const& topic_entry : topic_map.left) {
    topics.push_back(topic_entry.second);
  }

  if (RdKafka::ErrorCode error_code = consumer->subscribe(topics); error_code) {
    app.logger().error(std::format("Error subscribing to {} topics: {}",
                                   topics.size(),
                                   RdKafka::err2str(error_code)));
    throw Poco::ApplicationException(RdKafka::err2str(error_code));
  }
  running = true;
}

KafkaReader::~KafkaReader()
{
  consumer->close();
  delete consumer;
}

std::vector<RdKafka::Message*>
KafkaReader::consume_batch(RdKafka::KafkaConsumer* consumer,
                           size_t batch_size,
                           int timeout)
{
  std::vector<RdKafka::Message*> messages;
  messages.reserve(batch_size);
  int64_t end = now_as_int64() + timeout;
  int remaining_time = timeout;
  Poco::Util::Application& app = Poco::Util::Application::instance();

  while (messages.size() < batch_size) {
    RdKafka::Message* message = consumer->consume(remaining_time);

    switch (message->err()) {
      case RdKafka::ERR__TIMED_OUT:
        delete message;
        return messages;
      case RdKafka::ERR_NO_ERROR:
        messages.push_back(message);
        break;
      default:
        app.logger().error(
          std::format("%% Consumer error: {}", message->errstr()));
        running = false;
        delete message;
        return messages;
    }

    remaining_time = end - now_as_int64();

    if (remaining_time < 0)
      break;
  }

  return messages;
}

void
KafkaReader::run()
{
  Poco::Util::Application& app = Poco::Util::Application::instance();

  while (running) {
    Poco::Thread::sleep(200);
    auto messages = consume_batch(consumer, batch_size, timeout);
    if (messages.size() > 0)
      app.logger().information(
        std::format("Accumulated {} messages", messages.size()));

    for (auto& message : messages) {
      app.logger().information(std::format("Message in {} [{}] at offset {}",
                                           message->topic_name(),
                                           message->partition(),
                                           message->offset()));
      std::string data(static_cast<const char*>(message->payload()));
      if (topic_map.right.at(message->topic_name()) == "load") {
        // Check if data is type bag and parses out correctly
        load_queue->enqueue(data);
        app.logger().debug(std::format(
          "Queue size: {}. Attempting {}", load_queue->size_approx(), data));
      } else {
        app.logger().error(std::format(
          "Unsupported topic: {}", topic_map.right.at(message->topic_name())));
      }
      delete message;
    }
  }
}
