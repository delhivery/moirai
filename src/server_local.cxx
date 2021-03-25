#include "concurrentqueue.h"
#include "date_utils.hxx"
#include "file_reader.hxx"
#include "format.hxx"
#include "graph_helpers.hxx"
#include "kafka_reader.hxx"
#include "search_writer.hxx"
#include "server.hxx"
#include "solver_wrapper.hxx"
#include "transportation.hxx"
#include <Poco/Util/HelpFormatter.h>
#include <numeric>

void
Moirai::display_help()
{
  Poco::Util::HelpFormatter help_formatter(options());
  help_formatter.setCommand(commandName());
  help_formatter.setUsage("OPTIONS");
  help_formatter.setHeader(
    "System directed path prediction for transportation systems");
  help_formatter.format(std::cout);
}

void
Moirai::initialize(Poco::Util::Application& self)
{
  loadConfiguration();
  Poco::Util::ServerApplication::initialize(self);
  logger().debug("Starting up");
}

void
Moirai::uninitialize()
{
  logger().debug("Shutting down");
  Poco::Util::ServerApplication::uninitialize();
}

void
Moirai::reinitialize()
{}

void
Moirai::defineOptions(Poco::Util::OptionSet& options)
{
  options.addOption(
    Poco::Util::Option(
      "help", "h", "display help information on command line arguments")
      .required(false)
      .repeatable(false)
      .callback(
        Poco::Util::OptionCallback<Moirai>(this, &Moirai::handle_help)));

  options.addOption(Poco::Util::Option("route-api", "a", "Route dump API")
                      .required(true)
                      .repeatable(false)
                      .argument("<uri>", true)
                      .callback(Poco::Util::OptionCallback<Moirai>(
                        this, &Moirai::set_route_api)));

  options.addOption(
    Poco::Util::Option("facility-timings", "b", "Path to facility timings file")
      .required(true)
      .repeatable(false)
      .argument("<path>", true)
      .callback(Poco::Util::OptionCallback<Moirai>(
        this, &Moirai::set_facility_timings_file)));

  options.addOption(Poco::Util::Option("facility-api", "c", "Facility dump API")
                      .required(true)
                      .repeatable(false)
                      .argument("<uri>", true)
                      .callback(Poco::Util::OptionCallback<Moirai>(
                        this, &Moirai::set_facility_api)));

  options.addOption(Poco::Util::Option("route-token", "e", "Route API Token")
                      .required(true)
                      .repeatable(false)
                      .argument("<uri>", true)
                      .callback(Poco::Util::OptionCallback<Moirai>(
                        this, &Moirai::set_route_token)));

  options.addOption(
    Poco::Util::Option("facility-topic", "f", "Facility data topic")
      .required(true)
      .repeatable(false)
      .argument("<topic_name>", true)
      .callback(
        Poco::Util::OptionCallback<Moirai>(this, &Moirai::set_node_topic)));

  options.addOption(
    Poco::Util::Option("search-index", "i", "Elasticsearch index")
      .required(true)
      .repeatable(false)
      .argument("<string>", true)
      .callback(
        Poco::Util::OptionCallback<Moirai>(this, &Moirai::set_search_index)));

  options.addOption(Poco::Util::Option("kafka-broker", "k", "Kafka broker url")
                      .required(true)
                      .repeatable(true)
                      .argument("<broker_uri:broker_port>", true)
                      .callback(Poco::Util::OptionCallback<Moirai>(
                        this, &Moirai::set_broker_url)));

  options.addOption(
    Poco::Util::Option("facility-token", "l", "Facility API Token")
      .required(true)
      .repeatable(false)
      .argument("<uri>", true)
      .callback(Poco::Util::OptionCallback<Moirai>(
        this, &Moirai::set_facility_api_token)));

  options.addOption(
    Poco::Util::Option("package-topic", "p", "Package data topic")
      .required(true)
      .repeatable(false)
      .argument("<topic_name>", true)
      .callback(
        Poco::Util::OptionCallback<Moirai>(this, &Moirai::set_load_topic)));

  options.addOption(Poco::Util::Option("route-topic", "r", "Route data topic")
                      .required(true)
                      .repeatable(false)
                      .argument("<topic_name>", true)
                      .callback(Poco::Util::OptionCallback<Moirai>(
                        this, &Moirai::set_edge_topic)));

  options.addOption(Poco::Util::Option("search-uri", "s", "Elasticsearch URI")
                      .required(true)
                      .repeatable(false)
                      .argument("<es_uri>", true)
                      .callback(Poco::Util::OptionCallback<Moirai>(
                        this, &Moirai::set_search_uri)));

  options.addOption(
    Poco::Util::Option(
      "batch-timeout", "t", "Seconds to wait before processing batch")
      .required(false)
      .repeatable(false)
      .argument("<milliseconds>", true)
      .callback(
        Poco::Util::OptionCallback<Moirai>(this, &Moirai::set_batch_timeout)));

  options.addOption(
    Poco::Util::Option("search-user", "u", "Elasticsearch username")
      .required(true)
      .repeatable(false)
      .argument("<string>", true)
      .callback(Poco::Util::OptionCallback<Moirai>(
        this, &Moirai::set_search_username)));
  options.addOption(
    Poco::Util::Option("search-pass", "w", "Elasticsearch password")
      .required(true)
      .repeatable(false)
      .argument("<string>", true)
      .callback(Poco::Util::OptionCallback<Moirai>(
        this, &Moirai::set_search_password)));

  options.addOption(Poco::Util::Option("batch-size",
                                       "z",
                                       "Number of documents to fetch per batch")
                      .required(false)
                      .repeatable(false)
                      .argument("<int>", true)
                      .callback(Poco::Util::OptionCallback<Moirai>(
                        this, &Moirai::set_batch_size)));
}

void
Moirai::set_facility_timings_file(const std::string& name,
                                  const std::string& value)
{
  facility_timings_filename = value;
}

void
Moirai::set_facility_api(const std::string& name, const std::string& value)
{
  facility_uri = value;
}

void
Moirai::set_facility_api_token(const std::string& name,
                               const std::string& value)
{
  facility_token = value;
}

void
Moirai::set_route_api(const std::string& name, const std::string& value)
{
  route_uri = value;
}

void
Moirai::set_route_token(const std::string& name, const std::string& value)
{
  route_token = value;
}

void
Moirai::set_search_uri(const std::string& name, const std::string& value)
{
  search_uri = value;
}

void
Moirai::set_search_username(const std::string& name, const std::string& value)
{
  search_user = value;
}

void
Moirai::set_search_password(const std::string& name, const std::string& value)
{
  search_pass = value;
}

void
Moirai::set_search_index(const std::string& name, const std::string& value)
{
  search_index = value;
}

void
Moirai::set_batch_timeout(const std::string& name, const std::string& value)
{
  timeout = std::atoi(value.c_str());
}

void
Moirai::set_batch_size(const std::string& name, const std::string& value)
{
  batch_size = std::atoi(value.c_str());
}

void
Moirai::set_edge_topic(const std::string& name, const std::string& value)
{
  topic_map.insert(StringToStringMap::value_type("edge", value));
}

void
Moirai::set_node_topic(const std::string& name, const std::string& value)
{
  topic_map.insert(StringToStringMap::value_type("node", value));
}

void
Moirai::set_load_topic(const std::string& name, const std::string& value)
{
  topic_map.insert(StringToStringMap::value_type("load", value));
}

void
Moirai::set_broker_url(const std::string& name, const std::string& value)
{
  broker_url.push_back(value);
}

void
Moirai::handle_help(const std::string& name, const std::string& value)
{
  help_requested = true;
  display_help();
  stopOptionsProcessing();
}

int
Moirai::main(const ArgVec& arg)
{

  if (!help_requested) {
    moodycamel::ConcurrentQueue<std::string>* node_queue =
      new moodycamel::ConcurrentQueue<std::string>();
    moodycamel::ConcurrentQueue<std::string>* edge_queue =
      new moodycamel::ConcurrentQueue<std::string>();
    moodycamel::ConcurrentQueue<std::string>* load_queue =
      new moodycamel::ConcurrentQueue<std::string>();
    moodycamel::ConcurrentQueue<std::string>* solution_queue =
      new moodycamel::ConcurrentQueue<std::string>();
    SolverWrapper wrapper(node_queue,
                          edge_queue,
                          load_queue,
                          solution_queue,
                          facility_uri,
                          facility_token,
                          route_uri,
                          route_token,
                          facility_timings_filename);
    FileReader reader("/home/delhivery/WorkSpace/new_ep/moirai/fixtures/inputs.json", load_queue);
    SearchWriter writer(Poco::URI(search_uri),
                        search_user,
                        search_pass,
                        search_index,
                        solution_queue);
    
    KafkaReader kreader(
      std::accumulate(broker_url.begin(),
                      broker_url.end(),
                      std::string{},
                      [](const std::string& acc, const std::string& arg) {
                        return acc.empty() ? arg
                                           : moirai::format("{},{}", acc, arg);
                      }),
      batch_size,
      timeout,
      topic_map,
      node_queue,
      edge_queue,
      load_queue);

    std::vector<Poco::Thread> threads(4);
    try {
      threads[0].start(reader);
      threads[1].start(writer);
      threads[2].start(wrapper);
      threads[3].start(kreader);
    } catch (const std::exception& exc) {
      logger().error(moirai::format("MAIN: Error occurred: {}", exc.what()));
    }
    waitForTerminationRequest();
    for (auto& thread : threads)
      thread.join();
  }
  return Poco::Util::Application::EXIT_OK;
}

POCO_SERVER_MAIN(Moirai);
