#include <Poco/DateTimeFormatter.h>
#include <Poco/Exception.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPCredentials.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Runnable.h>
#include <Poco/Task.h>
#include <Poco/TaskManager.h>
#include <Poco/Util/Application.h>
#include <Poco/Util/HelpFormatter.h>
#include <Poco/Util/Option.h>
#include <Poco/Util/OptionCallback.h>
#include <Poco/Util/OptionSet.h>
#include <Poco/Util/ServerApplication.h>
#include <Poco/Util/Subsystem.h>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <librdkafka/rdkafkacpp.h>
#include <numeric>

#ifdef __cpp_lib_format
#include <format>
#else
#include <fmt/core.h>
namespace std {
using fmt::format;
};
#endif

static int64_t now() {
  auto n = std::chrono::system_clock::now();
  return n.time_since_epoch().count() / 1000 / 1000;
}

class KafkaReader : public Poco::Runnable {
private:
  std::string broker_url;
  RdKafka::Conf *config;
  RdKafka::KafkaConsumer *consumer;
  const uint16_t batch_size;
  const uint16_t timeout;
  static const std::string consumer_group;
  std::vector<std::string> topics;
  std::atomic<bool> running;

public:
  KafkaReader(const std::string broker_url, const uint16_t batch_size,
              const uint16_t timeout, const std::vector<std::string> &topics)
      : Poco::Runnable(), broker_url(broker_url), batch_size(batch_size),
        timeout(timeout), topics(std::move(topics)) {
    Poco::Util::Application &app = Poco::Util::Application::instance();
    app.logger().information("Configuring kafka reader");
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
      app.logger().error(
          std::format("Error setting group id: {}", error_string));
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

    if (RdKafka::ErrorCode error_code = consumer->subscribe(topics);
        error_code) {
      app.logger().error(std::format("Error subscribing to {} topics: {}",
                                     topics.size(),
                                     RdKafka::err2str(error_code)));
      throw Poco::ApplicationException(RdKafka::err2str(error_code));
    }
    running = true;
  }

  ~KafkaReader() {
    consumer->close();
    delete consumer;
  }

  std::vector<RdKafka::Message *>
  consume_batch(RdKafka::KafkaConsumer *consumer, size_t batch_size,
                int timeout) {
    std::vector<RdKafka::Message *> messages;
    messages.reserve(batch_size);
    int64_t end = now() + timeout;
    int remaining_time = timeout;
    Poco::Util::Application &app = Poco::Util::Application::instance();

    while (messages.size() < batch_size) {
      RdKafka::Message *message = consumer->consume(remaining_time);

      switch (message->err()) {
      case RdKafka::ERR__TIMED_OUT:
        delete message;
        return messages;
      case RdKafka::ERR_NO_ERROR:
        messages.push_back(message);
        break;
      default:
        app.logger().error(
            std::format("%% Consumer error: ", message->errstr()));
        running = false;
        delete message;
        return messages;
      }

      remaining_time = end - now();

      if (remaining_time < 0)
        break;
    }

    return messages;
  }

  virtual void run() {
    Poco::Util::Application &app = Poco::Util::Application::instance();

    while (running) {
      Poco::Thread::sleep(200);
      auto messages = consume_batch(consumer, batch_size, timeout);
      app.logger().information(
          std::format("Accumulated {} messages", messages.size()));

      for (auto &message : messages) {
        app.logger().information(std::format(
            "Message in {} [{}] at offset {}", message->topic_name(),
            message->partition(), message->offset()));
        delete message;
      }
    }
  }
};

class SearchWriter : public Poco::Runnable {
private:
  const std::string search_user;
  const std::string search_pass;
  const std::string search_index;

public:
  SearchWriter(std::string &search_url, std::string &search_user,
               std::string &search_pass, std::string &search_index)
      : Poco::Runnable(), search_user(std::move(search_user)),
        search_pass(std::move(search_pass)),
        search_index(std::move(search_index)) {}

  virtual void run() {
    Poco::URI search_uri(search_url);
    std::string search_path(search_uri.getPathAndQuery());
    Poco::Net::HTTPCredentials credentials(search_user, search_pass);
    Poco::Net::HTTPClientSession session(search_uri.getHost(),
                                         search_uri.getPort());
    while (true) {
      Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST,
    }
  }
};

const std::string KafkaReader::consumer_group = "MOIRAI";

class Moirai : public Poco::Util::ServerApplication {
private:
  std::vector<std::string> broker_url;
  std::map<std::string, std::string> topic_map;
  uint16_t batch_size;
  uint16_t timeout;

public:
  Moirai() {}

  ~Moirai() {}

private:
  bool help_requested;

  void display_help() {
    Poco::Util::HelpFormatter help_formatter(options());
    help_formatter.setCommand(commandName());
    help_formatter.setUsage("OPTIONS");
    help_formatter.setHeader(
        "System directed path prediction for transportation systems");
    help_formatter.format(std::cout);
  }

protected:
  void initialize(Poco::Util::Application &self) {
    loadConfiguration();
    Poco::Util::ServerApplication::initialize(self);
    logger().information("Starting up");
  }

  void uninitialize() {
    logger().information("Shutting down");
    Poco::Util::ServerApplication::uninitialize();
  }

  void reinitialize() {}

  void defineOptions(Poco::Util::OptionSet &options) {
    options.addOption(
        Poco::Util::Option("help", "h",
                           "display help information on command line arguments")
            .required(false)
            .repeatable(false)
            .callback(Poco::Util::OptionCallback<Moirai>(
                this, &Moirai::handle_help)));

    options.addOption(
        Poco::Util::Option("kafka-broker", "k", "Kafka broker url")
            .required(true)
            .repeatable(true)
            .callback(Poco::Util::OptionCallback<Moirai>(
                this, &Moirai::set_broker_url)));

    options.addOption(
        Poco::Util::Option("batch-timeout", "t",
                           "Seconds to wait before processing batch")
            .required(false)
            .repeatable(false)
            .callback(Poco::Util::OptionCallback<Moirai>(
                this, &Moirai::set_batch_timeout)));

    options.addOption(
        Poco::Util::Option("batch-size", "s",
                           "Number of documents to fetch per batch")
            .required(false)
            .repeatable(false)
            .callback(Poco::Util::OptionCallback<Moirai>(
                this, &Moirai::set_batch_size)));

    options.addOption(Poco::Util::Option("route-topic", "r", "Route data topic")
                          .required(true)
                          .repeatable(false)
                          .callback(Poco::Util::OptionCallback<Moirai>(
                              this, &Moirai::set_edge_topic)));

    options.addOption(
        Poco::Util::Option("facility-topic", "f", "Facility data topic")
            .required(true)
            .repeatable(false)
            .callback(Poco::Util::OptionCallback<Moirai>(
                this, &Moirai::set_node_topic)));

    options.addOption(
        Poco::Util::Option("package-topic", "p", "Package data topic")
            .required(true)
            .repeatable(false)
            .callback(Poco::Util::OptionCallback<Moirai>(
                this, &Moirai::set_load_topic)));
  }

  void set_batch_timeout(const std::string &name, const std::string &value) {
    timeout = std::atoi(value.c_str());
  }

  void set_batch_size(const std::string &name, const std::string &value) {
    batch_size = std::atoi(value.c_str());
  }

  void set_edge_topic(const std::string &name, const std::string &value) {
    topic_map["edge"] = value;
    logger().debug(std::format("Set route topic to {}", value));
  }

  void set_node_topic(const std::string &name, const std::string &value) {
    topic_map["node"] = value;
    logger().debug(std::format("Set center topic to  {}", value));
  }

  void set_load_topic(const std::string &name, const std::string &value) {
    topic_map["node"] = value;
    logger().debug(std::format("Set package topic to {}", value));
  }

  void set_broker_url(const std::string &name, const std::string &value) {
    broker_url.push_back(value);
    logger().debug(std::format(
        "Set kafka broker url to {}",
        std::accumulate(broker_url.begin(), broker_url.end(), std::string{})));
  }

  void handle_help(const std::string &name, const std::string &value) {
    help_requested = true;
    display_help();
    stopOptionsProcessing();
  }

  int main(const ArgVec &arg) {

    if (!help_requested) {
      KafkaReader reader(
          std::accumulate(broker_url.begin(), broker_url.end(), std::string{}),
          batch_size, timeout, std::vector<std::string>{});
      Poco::Thread thread;
      thread.start(reader);
      thread.join();
    }
    return Poco::Util::Application::EXIT_OK;
  }
};

POCO_SERVER_MAIN(Moirai);
