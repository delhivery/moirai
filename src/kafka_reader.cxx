#include "kafka_reader.hxx"
#include <Poco/Util/ServerApplication.h>
#include <fmt/format.h>
// #include "date_utils.hxx"

const std::string KafkaReader::consumerGroup = "CLOTHO";

KafkaReader::KafkaReader(const std::string& brokerUrl,
                         const std::string& topic,
                         const uint16_t timeout,
                         queue_t* qPtr,
                         size_t batchSize)
  : Producer(qPtr, batchSize)
  , mTimeout(timeout)
{
  mLogger = Poco::Logger::get("kafka-reader");
  mLogger.debug("Configuring kafka reader");
  RdKafka::Conf* config = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

  std::string errorString;

  if (config->set("enable.partition.eof", "false", errorString) !=
      RdKafka::Conf::CONF_OK) {
    mLogger.error(fmt::format("Error enabling partition eof: {}", errorString));
    throw Poco::ApplicationException(errorString);
  }

  if (config->set("group.id", consumerGroup, errorString) !=
      RdKafka::Conf ::CONF_OK) {
    mLogger.error(fmt::format("Error setting group id: {}", errorString));
    throw Poco::ApplicationException(errorString);
  }

  if (config->set("bootstrap.servers", brokerUrl, errorString) !=
      RdKafka::Conf::CONF_OK) {
    mLogger.error(fmt::format("Error bootstrapping servers. {}", errorString));
    throw Poco::ApplicationException(errorString);
  }

  if (config->set("auto.offset.reset", "smallest", errorString) !=
      RdKafka::Conf::CONF_OK) {
    mLogger.error(fmt::format("Error setting offset: {}", errorString));
  }
  mKafkaConsumerPtr = RdKafka::KafkaConsumer::create(config, errorString);

  if (mKafkaConsumerPtr == nullptr) {
    mLogger.error(fmt::format("Error creating consumer. {}", errorString));
    throw Poco::ApplicationException(errorString);
  }

  delete config;

  if (RdKafka::ErrorCode errorCode = mKafkaConsumerPtr->subscribe({ topic });
      errorCode) {
    mLogger.error(fmt::format("Error subscribing to topic<{}>: {}",
                              topic,
                              RdKafka::err2str(errorCode)));
    throw Poco::ApplicationException(RdKafka::err2str(errorCode));
  }
  mRunning = true;
}

KafkaReader::~KafkaReader()
{
  mKafkaConsumerPtr->close();
  delete mKafkaConsumerPtr;
}

auto
KafkaReader::fetch() -> nlohmann::json::array
{
  nlohmann::json::array messages;
  messages.reserve(mBatchSize);

  while (messages.size() < mBatchSize) {
    auto message = mKafkaConsumerPtr->consumer(mTimeout);

    switch (message->err()) {
      case RdKafka::ERR__TIMED_OUT:
        delete message;
        break;
      case RdKafka::ERR_NO_ERROR:
        nlohmann::json entry = message->payload();
        messages.push_back(entry);
        break;
      default:
        mLogger.error(fmt::format("Consumer error: {}", message->errstr()));
        delete message;
        break;
    }
  }
  return messages;
}
