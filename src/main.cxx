#include <iostream>
#include <string>
#include <vector>

#ifdef __cpp_lib_format
#include <format>
#else
#include <fmt/core.h>
namespace std {
using fmt::format;
};
#endif

#include <librdkafka/rdkafkacpp.h>

#include <Poco/Util/Option.h>
#include <Poco/Util/OptionCallback.h>
#include <Poco/Util/OptionSet.h>

struct Solver;

// Reads from kafka, writes to dispatch_queue
struct DispatchWorker;

// Reads from redis, writes to center_queue
struct CenterWorker;

// Reads from redis, writes to route_queue
struct RouteWorker;

static std::atomic<int> run = 1;
static bool exit_eof = false;
static int eof_count = 0;
static int partitions_count = 0;
static int verbosity = 1;
static int message_count = 0;
static int64_t message_bytes = 0;

static void
sigterm(int signal)
{
  run = 0;
}

class ContainerReaderRebalanceCallback : public RdKafka::RebalanceCb
{
private:
  static void part_list_print(
    const std::vector<RdKafka::TopicPartition*>& partitions)
  {

    for (unsigned int idx = 0; idx < partitions.size(); ++idx)
      std::cerr << std::format(
        "{}[{}], ", partitions[idx]->topic(), partitions[idx]->partition());
    std::cerr << std::endl;
  }

public:
  void rebalance_cb(RdKafka::KafkaConsumer* consumer,
                    RdKafka::ErrorCode err,
                    std::vector<RdKafka::TopicPartition*>& partitions)
  {
    std::cerr << fmt::format("RebalanceCb: {}; ", RdKafka::err2str(err));
    part_list_print(partitions);

    RdKafka::Error* error = NULL;
    RdKafka::ErrorCode return_error = RdKafka::ERR_NO_ERROR;

    if (err == RdKafka::ERR__ASSIGN_PARTITIONS) {
      if (consumer->rebalance_protocol() == "COOPERATIVE")
        error = consumer->incremental_assign(partitions);
      else
        return_error = consumer->assign(partitions);
      partitions_count += (int)partitions.size();
    } else {
      if (consumer->rebalance_protocol() == "COOPERATIVE") {
        error = consumer->incremental_unassign(partitions);
        partitions_count -= (int)partitions.size();
      } else {
        return_error = consumer->unassign();
        partitions_count = 0;
      }
    }
    eof_count = 0;

    if (error) {
      std::cerr << std::format("Incremental assign failed: {}", error->str())
                << std::endl;
      delete error;
    } else if (return_error) {
      std::cerr << std::format("Assign failed {}",
                               RdKafka::err2str(return_error))
                << std::endl;
    }
  }

  void msg_consumer(RdKafka::Message* message, void* opaque)
  {
    switch (message->err()) {
      case RdKafka::ERR__TIMED_OUT:
        message_count++;
        message_bytes += message->len();

        if (verbosity >= 3)
          std::cerr << std::format("Read message at offset {}",
                                   message->offset())
                    << std::endl;
        RdKafka::MessageTimestamp timestamp;
        timestamp = message->timestamp();

        if (verbosity >= 2 &&
            timestamp.type !=
              RdKafka::MessageTimestamp::MSG_TIMESTAMP_NOT_AVAILABLE) {
          std::string timestamp_name = "?";
          if (timestamp.type ==
              RdKafka::MessageTimestamp::MSG_TIMESTAMP_CREATE_TIME)
            timestamp_name = "create time";
          else if (timestamp.type ==
                   RdKafka::MessageTimestamp::MSG_TIMESTAMP_LOG_APPEND_TIME)
            timestamp_name = "log append time";
          std::cout << std::format("Timestamp: {} {}",
                                   timestamp_name,
                                   timestamp.timestamp)
                    << std::endl;
        }

        if (verbosity >= 2 && message->key()) {
          std::cout << std::format("Key: {}", *message->key()) << std::endl;
        }

        if (verbosity >= 1) {
          std::cout << fmt::format("{}{}",
                                   static_cast<int>(message->len()),
                                   static_cast<const char*>(message->payload()))
                    << std::endl;
        }
        break;
      case RdKafka::ERR__PARTITION_EOF:
        if (exit_eof && ++eof_count == partitions_count) {
          std::cerr << std::format("EOF reached for all {} partition(s)",
                                   partitions_count)
                    << std::endl;
          run = 0;
        }
        break;
      case RdKafka::ERR__UNKNOWN_TOPIC:
      case RdKafka::ERR__UNKNOWN_PARTITION:
        std::cerr << std::format("Consume failed {}", message->errstr())
                  << std::endl;
        run = 0;
        break;
      default:
        std::cerr << std::format("Consume failed: {}", message->errstr())
                  << std::endl;
        run = 0;
    }
  }
};

class ContainerReaderApplication : public Poco::Util::Application
{
protected:
  void initialize(Poco::Util::Application& application)
  {
    this->loadConfiguration();
    Poco::Util::Application::initialize(application);
  }

  void uninitialize() { Poco::Util::Application::uninitialize(); }

  void defineOptions(Poco::Util::OptionSet& options)
  {
    Poco::Util::Application::defineOptions(options);

    options.addOption(
      Poco::Util::Option("optionval", "", "Some value")
        .required(false)
        .repeatable(false)
        .argument("<the value>", true)
        .callback(Poco::Util::OptionCallback<ContainerReaderApplication>(
          this, &ContainerReaderApplication::handleOptionX)));
  }

  void handleOptionX(const std::string& name, const std::string& value)
  {
    std::cout << std::format("Setting option {} to {}", name, value)
              << std::endl;
    this->config().setString(name, value);
    std::cout << std::format("The option value is now {}",
                             this->config().getString(name))
              << std::endl;
  }

  int main(const std::vector<std::string>& arguments)
  {
    std::cout << std::format("We are now in main. Option is ",
                             this->config().getstring("optionval"))
              << std::endl;
  }
};

POCO_APP_MAIN(ContainerReaderApplication);

int
main(int argc, char* argv[])
{
  std::string brokers = "localhost";
  std::string error_string;
  std::string topic;
  std::string mode;
  std::string debug;
  std::vector<std::string> topics;

  RdKafka::Conf* config = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
  RdKafka::Conf* tconf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);

  ContainerReaderRebalanceCallback container_reader_callback;
  config->set("rebalance_cb", &container_reader_callback, error_string);

  return 0;
}
