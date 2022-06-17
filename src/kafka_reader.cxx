#include "kafka_reader.hxx"
#include <Poco/Util/ServerApplication.h>
#include <fmt/format.h>
// #include "date_utils.hxx"

const std::string KafkaReader::consumerGroup = "CLOTHO";

KafkaReader::KafkaReader(const std::string& brokerUrl,
                         const uint16_t batchSize,
                         const uint16_t timeout,
                         std::string topic,
                         moodycamel::ConcurrentQueue<std::string>* loadQueuePtr)
  : ScanReader(loadQueuePtr)
  , mBrokerUrl(brokerUrl)
  , mConsumerPtr(nullptr)
  , batchSize(batchSize)
  , timeout(timeout)
  , mTopic(std::move(topic))
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
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
  mConsumerPtr = RdKafka::KafkaConsumer::create(config, errorString);

  if (mConsumerPtr == nullptr) {
    mLogger.error(fmt::format("Error creating consumer. {}", errorString));
    throw Poco::ApplicationException(errorString);
  }

  delete config;

  if (RdKafka::ErrorCode errorCode = mConsumerPtr->subscribe({ mTopic });
      errorCode) {
    mLogger.error(fmt::format("Error subscribing to topic<{}>: {}",
                              mTopic,
                              RdKafka::err2str(errorCode)));
    throw Poco::ApplicationException(RdKafka::err2str(errorCode));
  }
  mRunning = true;
}

KafkaReader::~KafkaReader()
{
  mConsumerPtr->close();
  delete mConsumerPtr;
}

auto
KafkaReader::consume_batch(size_t batchSize, int maxWaitMS)
  -> std::vector<RdKafka::Message*>
{
  std::vector<RdKafka::Message*> messages;
  messages.reserve(batchSize);

  Poco::Util::Application& app = Poco::Util::Application::instance();

  while (messages.size() < batchSize) {
    RdKafka::Message* message = mConsumerPtr->consume(maxWaitMS);

    switch (message->err()) {
      case RdKafka::ERR__TIMED_OUT:
        delete message;
        return messages;
      case RdKafka::ERR_NO_ERROR:
        messages.push_back(message);
        break;
      default:
        mLogger.error(fmt::format("%% Consumer error: {}", message->errstr()));
        mRunning = false;
        delete message;
        return messages;
    }
  }

  return messages;
}

void
KafkaReader::run()
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  const uint64_t sleepFor = 200;
  const uint64_t maxRecords = 10240;

  while (mRunning) {
    Poco::Thread::sleep(sleepFor);

    if (mloadQueuePtr->size_approx() > maxRecords) {
      continue;
    }

    auto messages = consume_batch(batchSize, timeout);

    if (not messages.empty()) {
      mLogger.debug(fmt::format("Accumulated {} messages", messages.size()));
    }

    for (auto& message : messages) {
      mLogger.debug(fmt::format("Message in {} [{}] at offset {}",
                                message->topic_name(),
                                message->partition(),
                                message->offset()));
      std::string data(static_cast<const char*>(message->payload()));
      mloadQueuePtr->enqueue(data);
      delete message;
    }
  }
}
