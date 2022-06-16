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
  Poco::Util::HelpFormatter helpFormatter(options());
  helpFormatter.setCommand(commandName());
  helpFormatter.setUsage("OPTIONS");
  helpFormatter.setHeader(
    "System directed path prediction for transportation systems");
  helpFormatter.format(std::cout);
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
                        this, &Moirai::set_node_file)));
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
      .callback(Poco::Util::OptionCallback<Moirai>(
        this, &Moirai::set_load_broker_size)));
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
      .callback(
        Poco::Util::OptionCallback<Moirai>(this, &Moirai::set_sync_uri)));
  options.addOption(
    Poco::Util::Option("sync-index", "si", "ES index to sync output to")
      .required(true)
      .repeatable(false)
      .argument("<index>", true)
      .group("sync")
      .callback(
        Poco::Util::OptionCallback<Moirai>(this, &Moirai::set_sync_idx)));
  options.addOption(
    Poco::Util::Option(
      "sync-username", "su", "Username credentials to ES to sync output to")
      .required(true)
      .repeatable(false)
      .argument("<username>", true)
      .group("sync")
      .callback(
        Poco::Util::OptionCallback<Moirai>(this, &Moirai::set_sync_user)));
  options.addOption(
    Poco::Util::Option(
      "sync-password", "sp", "Password credentials to ES to sync output to")
      .required(true)
      .repeatable(false)
      .argument("<password>", true)
      .group("sync")
      .callback(
        Poco::Util::OptionCallback<Moirai>(this, &Moirai::set_sync_pass)));
#endif
}

#ifdef WITH_NODE_FILE
void
Moirai::set_node_file(const std::string& name, const std::string& value)
{
  mNodeFile = value;
}
#else
void
Moirai::set_node_uri(const std::string& name, const std::string& value)
{
  mNodeUri = value;
}

void
Moirai::set_node_idx(const std::string& name, const std::string& value)
{
  mNodeIndex = value;
}

void
Moirai::set_node_user(const std::string& name, const std::string& value)
{
  mNodeAuthUser = value;
}

void
Moirai::set_node_pass(const std::string& name, const std::string& value)
{
  mNodeAuthPass = value;
}
#endif

#ifdef WITH_EDGE_FILE
void
Moirai::set_edge_file(const std::string& name, const std::string& value)
{
  mEdgeFile = value;
}
#else
void
Moirai::set_edge_uri(const std::string& name, const std::string& value)
{
  mEdgeUri = value;
}

void
Moirai::set_edge_auth(const std::string& name, const std::string& value)
{
  mEdgeToken = value;
}
#endif

#ifdef WITH_LOAD_FILE
void
Moirai::set_load_file(const std::string& name, const std::string& value)
{
  mLoadFile = value;
}
#else
void
Moirai::set_load_broker_uri(const std::string& name, const std::string& value)
{
  mLoadBrokerUris.emplace_back(value);
}

void
Moirai::set_load_broker_size(const std::string& name, const std::string& value)
{
  mLoadBatchSize = std::atoi(value.c_str());
}

void
Moirai::set_load_broker_timeout(const std::string& name,
                                const std::string& value)
{
  mLoadConsumerTimeout = std::atoi(value.c_str());
}

void
Moirai::set_load_topic(const std::string& name, const std::string& value)
{
  mLoadTopic = value;
}
#endif

#ifdef ENABLE_SYNC
void
Moirai::set_sync_uri(const std::string& name, const std::string& value)
{
  mSyncUri = value;
}

void
Moirai::set_sync_idx(const std::string& name, const std::string& value)
{
  mSyncIndex = value;
}

void
Moirai::set_sync_user(const std::string& name, const std::string& value)
{
  mSyncAuthUser = value;
}

void
Moirai::set_sync_pass(const std::string& name, const std::string& value)
{
  mSyncAuthPass = value;
}
#endif

void
Moirai::handle_help(const std::string& name, const std::string& value)
{
  mHelpRequested = true;
  display_help();
  stopOptionsProcessing();
}

auto
Moirai::main(const ArgVec& arg) -> int
{

  logger().information("Starting main");
  if (!mHelpRequested) {
    auto* loadQueuePtr = new moodycamel::ConcurrentQueue<std::string>();
    auto* solQueuePtr = new moodycamel::ConcurrentQueue<std::string>();
    {
      SolverWrapper wrapper(loadQueuePtr,
                            solQueuePtr
#ifdef WITH_NODE_FILE
                            ,
                            mNodeFile
#else
                            ,
                            mNodeUri,
                            mNodeIndex,
                            mNodeAuthUser,
                            mNodeAuthPass
#endif
#ifdef WITH_EDGE_FILE
                            ,
                            mEdgeFile
#else
                            ,
                            mEdgeUri,
                            mEdgeToken
#endif
      );

      std::shared_ptr<ScanReader> reader = nullptr;

#ifdef WITH_LOAD_FILE
      reader = std::make_shared<FileReader>(mLoadFile, loadQueuePtr);
#else
      reader = std::make_shared<KafkaReader>(
        std::accumulate(
          mLoadBrokerUris.begin(),
          mLoadBrokerUris.end(),
          std::string{},
          [](const std::string& acc, const std::string& brokerUri) {
            return acc.empty() ? brokerUri
                               : fmt::format("{},{}", acc, brokerUri);
          }),
        mLoadBatchSize,
        mLoadConsumerTimeout,
        mLoadTopic,
        loadQueuePtr);
#endif
      std::shared_ptr<SearchWriter> writer = nullptr;

#ifdef ENABLE_SYNC
      writer = std::make_shared<SearchWriter>(Poco::URI(mSyncUri),
                                              mSyncIndex,
                                              mSyncAuthPass,
                                              mSyncAuthPass,
                                              solQueuePtr);
#endif
      int16_t nThreads = std::thread::hardware_concurrency();
      nThreads = nThreads < 3 ? 3 : nThreads;
      std::vector<Poco::Thread> threads(nThreads);
      std::vector<std::shared_ptr<SolverWrapper>> secondary;
      secondary.reserve(nThreads - 3);

      try {
        threads[0].start(*reader);
        threads[1].start(*writer);
        threads[2].start(wrapper);

        for (int idx = 3; idx < nThreads; ++idx) {
          secondary.emplace_back(std::make_shared<SolverWrapper>(wrapper));
          threads[idx].start(*secondary[idx - 3]);
        }
      } catch (const std::exception& exc) {
        logger().error(fmt::format("MAIN: Error occurred: {}", exc.what()));
      }
      waitForTerminationRequest();
      logger().information("Termination requested");
      reader->mRunning = false;
      writer->running = false;
      wrapper.mRunning = false;

      for (auto& wrapper : secondary) {
        wrapper->mRunning = false;
      }

      for (auto& thread : threads) {
        thread.join();
      }
    }

    delete loadQueuePtr;
    delete loadQueuePtr;
    delete loadQueuePtr;
    delete solQueuePtr;
  }
  return Poco::Util::Application::EXIT_OK;
}

auto
main(int argc, char** argv) -> int
{
  Moirai app;
  return app.run(argc, argv);
}
