#include "producer_kafka.hxx"
#include <Poco/Util/ServerApplication.h>
#include <bits/types/struct_timeval.h>
#include <fmt/format.h>
// #include "date_utils.hxx"

const std::string MSKLoadProducer::CONSUMER_GROUP = "CLOTHO";

MSKLoadProducer::MSKLoadProducer(std::string_view brokerUrl,
                                 std::string_view topic, uint16_t timeout,
                                 queue_t *qPtr, size_t batchSize)
    : Producer(qPtr, batchSize), mTimeout(timeout) {
  logger().debug("Configuring kafka reader");
  RdKafka::Conf *config = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

  std::string errorString;

  if (config->set("enable.partition.eof", "false", errorString) !=
      RdKafka::Conf::CONF_OK) {
    logger().error(
        fmt::format("Error enabling partition eof: {}", errorString));
    throw Poco::ApplicationException(errorString);
  }

  if (config->set("group.id", CONSUMER_GROUP, errorString) !=
      RdKafka::Conf ::CONF_OK) {
    logger().error(fmt::format("Error setting group id: {}", errorString));
    throw Poco::ApplicationException(errorString);
  }

  if (config->set("bootstrap.servers", brokerUrl.data(), errorString) !=
      RdKafka::Conf::CONF_OK) {
    logger().error(fmt::format("Error bootstrapping servers. {}", errorString));
    throw Poco::ApplicationException(errorString);
  }

  if (config->set("auto.offset.reset", "smallest", errorString) !=
      RdKafka::Conf::CONF_OK) {
    logger().error(fmt::format("Error setting offset: {}", errorString));
  }
  mKafkaConsumerPtr = RdKafka::KafkaConsumer::create(config, errorString);

  if (mKafkaConsumerPtr == nullptr) {
    logger().error(fmt::format("Error creating consumer. {}", errorString));
    throw Poco::ApplicationException(errorString);
  }

  delete config;

  if (RdKafka::ErrorCode errorCode =
          mKafkaConsumerPtr->subscribe({topic.data()});
      errorCode) {
    logger().error(fmt::format("Error subscribing to topic<{}>: {}", topic,
                               RdKafka::err2str(errorCode)));
    throw Poco::ApplicationException(RdKafka::err2str(errorCode));
  }
}

MSKLoadProducer::~MSKLoadProducer() {
  mKafkaConsumerPtr->close();
  delete mKafkaConsumerPtr;
}

auto MSKLoadProducer::logger() const -> Poco::Logger & {
  return Poco::Logger::get("load-producer.kafka");
}

auto MSKLoadProducer::fetch() -> std::vector<json_t> {
  std::vector<json_t> messages;
  messages.reserve(mBatchSize);

  auto end =
      std::chrono::system_clock::now() + std::chrono::milliseconds(mTimeout);
  int remainingTimeout = mTimeout;

  while (messages.size() < mBatchSize) {
    auto *message = mKafkaConsumerPtr->consume(remainingTimeout);

    switch (message->err()) {
    case RdKafka::ERR__TIMED_OUT:
      delete message;
      return messages;
    case RdKafka::ERR_NO_ERROR:
      messages.emplace_back(static_cast<const char *>(message->payload()));
      break;
    default:
      logger().error(fmt::format("Consumer error: {}", message->errstr()));
      stop(true);
      delete message;
      return messages;
    }
    remainingTimeout = (end - std::chrono::system_clock::now()).count();

    if (remainingTimeout < 0) {
      break;
    }
  }
  return messages;
}
