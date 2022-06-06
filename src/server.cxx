#include "server.hxx"
#include "concurrentqueue.h"
#include "date_utils.hxx"
#include "file_reader.hxx"
#include "graph_helpers.hxx"
#include "kafka_reader.hxx"
#include "scan_reader.hxx"
#include "search_writer.hxx"
#include "solver_wrapper.hxx"
#include "transportation.hxx"
#include <Poco/Util/HelpFormatter.h>
#include <fmt/format.h>
#include <numeric>
#include <thread>

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
{
}

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

#ifdef WITH_NODE_FILE
  options.addOption(Poco::Util::Option("nodes-file", "n", "Nodes data file")
      .required(true)
      .repeatable(false)
      .argument("<filepath>", true)
      .group("node")
      .callback(Poco::Util::OptionCallback<Moirai>(
          this, &Moirai::set_node_file
      ));
#else
  options.addOption(Poco::Util::Option("nodes-uri", "n", "Nodes data API")
                      .required(true)
                      .repeatable(false)
                      .argument("<uri>", true)
                      .group("node")
                      .callback(Poco::Util::OptionCallback<Moirai>(
                        this, &Moirai::set_node_uri)));
  options.addOption(Poco::Util::Option("nodes-idx", "ni", "Nodes data index")
                      .required(true)
                      .repeatable(false)
                      .argument("<index>", true)
                      .group("node")
                      .callback(Poco::Util::OptionCallback<Moirai>(
                        this, &Moirai::set_node_idx)));
  options.addOption(Poco::Util::Option("node-user", "nu", "Nodes API user")
                      .required(true)
                      .repeatable(false)
                      .argument("<username>", true)
                      .group("node")
                      .callback(Poco::Util::OptionCallback<Moirai>(
                        this, &Moirai::set_node_user)));
  options.addOption(Poco::Util::Option("node-pass", "np", "Nodes API pass")
                      .required(true)
                      .repeatable(false)
                      .argument("<password>", true)
                      .group("node")
                      .callback(Poco::Util::OptionCallback<Moirai>(
                        this, &Moirai::set_node_user)));
#endif

#ifdef WITH_EDGE_FILE
  options.addOption(Poco::Util::Option("edge-file", "e", "Edges data file")
                      .required(true)
                      .repeatable(false)
                      .argument("<filepath>", true)
                      .group("edge")
                      .callback(Poco::Util::OptionCallback<Moirai>(
                        this, &Moirai::set_edge_file)));
#else
  options.addOption(Poco::Util::Option("edge-api", "e", "Edges data API")
                      .required(true)
                      .repeatable(false)
                      .argument("<uri>", true)
                      .group("edge")
                      .callback(Poco::Util::OptionCallback<Moirai>(
                        this, &Moirai::set_edge_uri)));
  options.addOption(Poco::Util::Option("edge-auth", "ea", "Edge data API auth")
                      .required(true)
                      .repeatable(false)
                      .argument("<token>", true)
                      .group("edge")
                      .callback(Poco::Util::OptionCallback<Moirai>(
                        this, &Moirai::set_edge_auth)));
#endif

#ifdef WITH_LOAD_FILE
  options.addOption(Poco::Util::Option("load-file", "l", "Loads data file")
                      .required(true)
                      .repeatable(false)
                      .argument("<filepath>", true)
                      .group("load")
                      .callback(Poco::Util::OptionCallback<Moirai>(
                        this, &Moirai::set_load_file)));
#else
  options.addOption(
    Poco::Util::Option("load-broker", "l", "Load data kafka broker")
      .required(true)
      .repeatable(true)
      .argument("<uri>", true)
      .group("load")
      .callback(Poco::Util::OptionCallback<Moirai>(
        this, &Moirai::set_load_broker_uri)));
  options.addOption(
    Poco::Util::Option("load-batch-size", "lz", "Load consumer batch size")
      .required(false)
      .repeatable(false)
      .argument("<int>", true)
      .group("load")
      .callback(
        Poco::Util::OptionCallback<Moirai>(this, &Moirai::set_load_broker_sz)));
  options.addOption(
    Poco::Util::Option("load-timeout", "ls", "Load consumer timeout")
      .required(false)
      .repeatable(false)
      .argument("<millis>", true)
      .group("load")
      .callback(Poco::Util::OptionCallback<Moirai>(
        this, &Moirai::set_load_broker_timeout)));
  options.addOption(Poco::Util::Option("load-topic", "lt", "Load data topic")
                      .required(true)
                      .repeatable(false)
                      .argument("<topic>", true)
                      .group("load")
                      .callback(Poco::Util::OptionCallback<Moirai>(
                        this, &Moirai::set_load_topic)));
#endif

#ifdef ENABLE_SYNC
  options.addOption(
    Poco::Util::Option("sync-uri", "s", "ES URI to sync output to")
      .required(true)
      .repeatable(false)
      .argument("<uri>", true)
      .group("sync")
      .callback(Poco::Util::OptionCallback<Moirai>(this, &Moirai::set_sync_uri)));
  options.addOption(
    Poco::Util::Option("sync-index", "si", "ES index to sync output to")
      .required(true)
      .repeatable(false)
      .argument("<index>", true)
      .group("sync")
      .callback(Poco::Util::OptionCallback<Moirai>(this, &Moirai::set_sync_idx)));
  options.addOption(
    Poco::Util::Option("sync-username", "su", "Username credentials to ES to sync output to")
      .required(true)
      .repeatable(false)
      .argument("<username>", true)
      .group("sync")
      .callback(Poco::Util::OptionCallback<Moirai>(this, &Moirai::set_sync_user)));
  options.addOption(
    Poco::Util::Option("sync-password", "sp", "Password credentials to ES to sync output to")
      .required(true)
      .repeatable(false)
      .argument("<password>", true)
      .group("sync")
      .callback(Poco::Util::OptionCallback<Moirai>(this, &Moirai::set_sync_pass)));
#endif
}

#ifdef WITH_NODE_FILE
void
Moirai::set_node_file(const std::string& name, const std::string& value)
{
  file_nodes = value;
}
#else
void
Moirai::set_node_uri(const std::string& name, const std::string& value)
{
  node_sync_uri = value;
}

void
Moirai::set_node_idx(const std::string& name, const std::string& value)
{
  node_sync_idx = value;
}

void
Moirai::set_node_user(const std::string& name, const std::string& value)
{
  node_sync_user = value;
}

void
Moirai::set_node_pass(const std::string& name, const std::string& value)
{
  node_sync_pass = value;
}
#endif

#ifdef WITH_EDGE_FILE
void
Moirai::set_edge_file(const std::string& name, const std::string& value)
{
  file_edges = value;
}
#else
void
Moirai::set_edge_uri(const std::string& name, const std::string& value)
{
  edge_uri = value;
}

void
Moirai::set_edge_auth(const std::string& name, const std::string& value)
{
  edge_token = value;
}
#endif

#ifdef WITH_LOAD_FILE
void
Moirai::set_load_file(const std::string& name, const std::string& value)
{
  file_loads = value;
}
#else
void
Moirai::set_load_broker_uri(const std::string& name, const std::string& value)
{
  load_broker_uris.emplace_back(value);
}

void
Moirai::set_load_broker_size(const std::string& name, const std::string& value)
{
  load_batch_size = std::atoi(value.c_str());
}

void
Moirai::set_load_broker_timeout(const std::string& name,
                                const std::string& value)
{
  load_consumer_timeout = std::atoi(value.c_str());
}

void
Moirai::set_load_topic(const std::string& name, const std::string& value)
{
  load_topic = value;
}
#endif

#ifdef ENABLE_SYNC
void
Moirai::set_sync_uri(const std::string& name, const std::string& value)
{
  sync_uri = value;
}

void
Moirai::set_sync_idx(const std::string& name, const std::string& value)
{
  sync_idx = value;
}

void
Moirai::set_sync_user(const std::string& name, const std::string& value)
{
  sync_user = value;
}

void
Moirai::set_sync_pass(const std::string& name, const std::string& value)
{
  sync_pass = value;
}
#endif

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

  logger().information("Starting main");
  if (!help_requested) {
    moodycamel::ConcurrentQueue<std::string>* load_queue =
      new moodycamel::ConcurrentQueue<std::string>();
    moodycamel::ConcurrentQueue<std::string>* solution_queue =
      new moodycamel::ConcurrentQueue<std::string>();
    {
      SolverWrapper wrapper(load_queue,
                            solution_queue,
#ifdef WITH_NODE_FILE
                            file_nodes,
#else
                            node_sync_uri,
                            node_sync_idx,
                            node_sync_user,
                            node_sync_pass,
#endif
#ifdef WITH_EDGE_FILE
                            file_edges,
#else
                            edge_uri,
                            edge_token,
#endif
      );

      std::shared_ptr<ScanReader> reader = nullptr;

#ifdef WITH_LOAD_FILE
      reader = std::make_shared<FileReader>(file_loads, load_queue);
#else
      reader = std::make_shared<KafkaReader>(
        std::accumulate(
          load_broker_uris.begin(),
          load_broker_uris.end(),
          std::string{},
          [](const std::string& acc, const std::string& broker_uri) {
            return acc.empty() ? broker_uri
                               : fmt::format("{},{}", acc, broker_uri);
          }),
        load_batch_size,
        load_consumer_timeout,
        load_topic,
        load_queue);
#endif
      std::shared_ptr<SearchWriter> writer = nullptr;

#ifdef ENABLE_SYNC
      writer = std::make_shared<SearchWriter>(
        Poco::URI(sync_uri), sync_idx, sync_user, sync_pass, solution_queue);
#endif
      int16_t num_threads = std::thread::hardware_concurrency();
      num_threads = num_threads < 3 ? 3 : num_threads;
      std::vector<Poco::Thread> threads(num_threads);
      std::vector<std::shared_ptr<SolverWrapper>> secondary;
      secondary.reserve(num_threads - 3);

      try {
        threads[0].start(*reader);
        threads[1].start(*writer);
        threads[2].start(wrapper);

        for (int idx = 3; idx < num_threads; ++idx) {
          secondary.emplace_back(load_queue, solution_queue, wrapper);
          threads[idx].start(*secondary[idx - 3]);
        }
      } catch (const std::exception& exc) {
        logger().error(fmt::format("MAIN: Error occurred: {}", exc.what()));
      }
      waitForTerminationRequest();
      logger().information("Termination requested");
      reader->running = false;
      writer.running = false;
      wrapper.running = false;

      for (auto& secondary_wrapper : secondary) {
        secondary_wrapper->running = false;
      }

      for (auto& thread : threads)
        thread.join();
    }

    delete node_queue;
    delete edge_queue;
    delete load_queue;
    delete solution_queue;
  }
  return Poco::Util::Application::EXIT_OK;
}

POCO_SERVER_MAIN(Moirai);
