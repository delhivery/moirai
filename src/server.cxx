module;

#include "blocking_queue.hxx"
#include <getopt.h>

module moirai.server;

import std;
import moirai.app;
import moirai.file_reader;
import moirai.http;
import moirai.kafka_reader;
import moirai.scan_reader;
import moirai.search_writer;
import moirai.solver_wrapper;
import moirai.utils;

void Moirai::display_help() const {
  std::cout << "System directed path prediction for transportation systems\n\n";
  std::cout << "Usage: " << m_command_name << " [options]\n\n";
  std::cout << "Required options:\n";
  std::cout << "  -a, --route-api <uri>\n";
  std::cout << "  -c, --facility-api <uri>\n";
  std::cout << "  -e, --route-token <token>\n";
  std::cout << "  -i, --search-index <string>\n";
  std::cout << "  -k, --kafka-broker <host:port> (repeatable)\n";
  std::cout << "  -l, --facility-token <token>\n";
  std::cout << "  -m, --kafka-config <key=value> (repeatable)\n";
  std::cout << "  -p, --package-topic <topic>\n";
  std::cout << "  -s, --search-uri <uri>\n";
  std::cout << "  -u, --search-user <string>\n";
  std::cout << "  -w, --search-pass <string>\n\n";
  std::cout << "Optional:\n";
  std::cout << "  -b, --facility-timings <path> (ignored; compatibility only)\n";
  std::cout << "  -f, --facility-topic <topic> (ignored; compatibility only)\n";
  std::cout << "  -h, --help\n";
  std::cout << "  --kafka-config supports SASL/MSK settings such as\n";
  std::cout
      << "     security.protocol=SASL_SSL,sasl.mechanisms=SCRAM-SHA-512,\n";
  std::cout << "     sasl.username=...,sasl.password=...\n";
  std::cout << "  -q, --query-from <path>\n";
  std::cout << "  -r, --route-topic <topic> (ignored; compatibility only)\n";
  std::cout << "  --search-writers <count>\n";
  std::cout << "  -t, --batch-timeout <milliseconds>\n";
  std::cout << "  -z, --batch-size <count>\n";
}

void Moirai::validate_options() const {
  auto require_value = [](const std::string &name,
                          const std::string &value) -> void {
    if (value.empty()) {
      throw std::runtime_error(std::format("Missing required option {}", name));
    }
  };

  require_value("--route-api", m_route_uri);
  require_value("--facility-api", m_facility_uri);
  require_value("--route-token", m_route_token);
  require_value("--facility-token", m_facility_token);
  require_value("--search-index", m_search_index);
  require_value("--search-uri", m_search_uri);
  require_value("--search-user", m_search_user);
  require_value("--search-pass", m_search_pass);

  if (!m_local_mode && m_broker_url.empty()) {
    throw std::runtime_error("Missing required option --kafka-broker");
  }

  if (!m_local_mode && !m_topic_map.contains_role("load")) {
    throw std::runtime_error("Missing required option --package-topic");
  }
  if (m_search_writer_threads == 0) {
    throw std::runtime_error("--search-writers must be greater than zero");
  }
}

void Moirai::parse_options(int argc, char **argv) {
  m_command_name = std::filesystem::path(argv[0]).filename().string();

  constexpr int SEARCH_WRITERS_OPTION = 1000;
  constexpr std::array<option, 20> long_options{{
      {.name = "help", .has_arg = no_argument, .flag = nullptr, .val = 'h'},
      {.name = "route-api",
       .has_arg = required_argument,
       .flag = nullptr,
       .val = 'a'},
      {.name = "query-from",
       .has_arg = required_argument,
       .flag = nullptr,
       .val = 'q'},
      {.name = "facility-timings",
       .has_arg = required_argument,
       .flag = nullptr,
       .val = 'b'},
      {.name = "facility-api",
       .has_arg = required_argument,
       .flag = nullptr,
       .val = 'c'},
      {.name = "route-token",
       .has_arg = required_argument,
       .flag = nullptr,
       .val = 'e'},
      {.name = "facility-topic",
       .has_arg = required_argument,
       .flag = nullptr,
       .val = 'f'},
      {.name = "search-index",
       .has_arg = required_argument,
       .flag = nullptr,
       .val = 'i'},
      {.name = "kafka-broker",
       .has_arg = required_argument,
       .flag = nullptr,
       .val = 'k'},
      {.name = "facility-token",
       .has_arg = required_argument,
       .flag = nullptr,
       .val = 'l'},
      {.name = "kafka-config",
       .has_arg = required_argument,
       .flag = nullptr,
       .val = 'm'},
      {.name = "package-topic",
       .has_arg = required_argument,
       .flag = nullptr,
       .val = 'p'},
      {.name = "route-topic",
       .has_arg = required_argument,
       .flag = nullptr,
       .val = 'r'},
      {.name = "search-uri",
       .has_arg = required_argument,
       .flag = nullptr,
       .val = 's'},
      {.name = "batch-timeout",
       .has_arg = required_argument,
       .flag = nullptr,
       .val = 't'},
      {.name = "search-user",
       .has_arg = required_argument,
       .flag = nullptr,
       .val = 'u'},
      {.name = "search-pass",
       .has_arg = required_argument,
       .flag = nullptr,
       .val = 'w'},
      {.name = "batch-size",
       .has_arg = required_argument,
       .flag = nullptr,
       .val = 'z'},
      {.name = "search-writers",
       .has_arg = required_argument,
       .flag = nullptr,
       .val = SEARCH_WRITERS_OPTION},
      {.name = nullptr, .has_arg = 0, .flag = nullptr, .val = 0},
  }};

  opterr = 0;
  optind = 1;

  while (true) {
    const int current = getopt_long(
        argc, argv, "ha:q:b:c:e:f:i:k:l:m:p:r:s:t:u:w:z:", long_options.data(),
        nullptr);

    if (current == -1) {
      break;
    }

    switch (current) {
    case 'h':
      m_help_requested = true;
      return;
    case 'a':
      m_route_uri = optarg;
      break;
    case 'q':
      m_scans_query_file = optarg;
      m_local_mode = true;
      break;
    case 'b':
      m_facility_timings_filename = optarg;
      break;
    case 'c':
      m_facility_uri = optarg;
      break;
    case 'e':
      m_route_token = optarg;
      break;
    case 'f':
      m_topic_map.insert("node", optarg);
      break;
    case 'i':
      m_search_index = optarg;
      break;
    case 'k':
      m_broker_url.emplace_back(optarg);
      break;
    case 'l':
      m_facility_token = optarg;
      break;
    case 'm': {
      const std::string argument = optarg;
      const auto separator = argument.find('=');
      if (separator == std::string::npos || separator == 0 ||
          separator == argument.size() - 1) {
        throw std::runtime_error("Invalid --kafka-config. Expected key=value");
      }
      m_kafka_properties[argument.substr(0, separator)] =
          argument.substr(separator + 1);
      break;
    }
    case 'p':
      m_topic_map.insert("load", optarg);
      break;
    case 'r':
      m_topic_map.insert("edge", optarg);
      break;
    case 's':
      m_search_uri = optarg;
      break;
    case 't':
      m_timeout = static_cast<std::uint16_t>(std::stoul(optarg));
      break;
    case 'u':
      m_search_user = optarg;
      break;
    case 'w':
      m_search_pass = optarg;
      break;
    case 'z':
      m_batch_size = static_cast<std::uint16_t>(std::stoul(optarg));
      break;
    case SEARCH_WRITERS_OPTION:
      m_search_writer_threads =
          static_cast<std::uint16_t>(std::stoul(optarg));
      break;
    default:
      throw std::runtime_error("Invalid command line option");
    }
  }

  if (optind < argc) {
    throw std::runtime_error(
        std::format("Unexpected positional argument {}", argv[optind]));
  }

  if (!m_help_requested) {
    validate_options();
  }
}

namespace {

constexpr std::size_t PIPELINE_QUEUE_CAPACITY = 4096;

} // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto Moirai::run_pipeline() -> int {
  auto &app = moirai::Application::instance();
  app.logger().information("Starting main");

  BlockingQueue<std::string> node_queue{1};
  BlockingQueue<std::string> edge_queue{1};
  BlockingQueue<std::string> load_queue{PIPELINE_QUEUE_CAPACITY};
  BlockingQueue<SearchDocument> solution_queue{PIPELINE_QUEUE_CAPACITY};

  SolverWrapper wrapper(
      SolverWrapper::RuntimeQueues{.node = &node_queue,
                                   .edge = &edge_queue,
                                   .load = &load_queue,
                                   .solution = &solution_queue},
      SolverWrapper::InitEndpoints{.node_uri = m_facility_uri,
                                   .node_token = m_facility_token,
                                   .edge_uri = m_route_uri,
                                   .edge_token = m_route_token},
      m_facility_timings_filename);

  std::shared_ptr<ScanReader> reader;
  if (m_local_mode) {
    reader = std::make_shared<FileReader>(m_scans_query_file, &load_queue);
    app.logger().set_level("debug");
  } else {
    std::string brokers;
    for (const auto &broker : m_broker_url) {
      if (!brokers.empty()) {
        brokers += ",";
      }
      brokers += broker;
    }

    reader = std::make_shared<KafkaReader>(
        brokers, m_batch_size, std::chrono::milliseconds{m_timeout},
        m_topic_map, m_kafka_properties,
        KafkaReader::QueueSet{
            .node = nullptr, .edge = nullptr, .load = &load_queue});
  }

  std::vector<std::shared_ptr<SearchWriter>> writers;
  writers.reserve(m_search_writer_threads);
  for (std::uint16_t index = 0; index < m_search_writer_threads; ++index) {
    writers.push_back(std::make_shared<SearchWriter>(
        moirai::parse_uri(m_search_uri), m_search_user, m_search_pass,
        m_search_index, &solution_queue));
  }

  const unsigned int hardware_threads =
      std::max(3U, std::thread::hardware_concurrency());
  const int solver_threads =
      std::max(1, static_cast<int>(hardware_threads) - 2);
  const int num_threads = solver_threads + 1 + m_search_writer_threads;
  std::atomic<int> active_solver_threads{solver_threads};
  app.logger().information(
      "Starting {} solver threads and {} search writer threads on {} hardware "
      "threads",
      solver_threads, m_search_writer_threads, hardware_threads);

  std::vector<std::shared_ptr<SolverWrapper>> secondary;
  secondary.reserve(std::max(0, solver_threads - 1));

  std::vector<std::jthread> threads;
  threads.reserve(num_threads);

  try {
    threads.emplace_back([&app, &reader, &node_queue, &edge_queue,
                          &load_queue](std::stop_token stop_token) -> void {
      try {
        reader->run(std::move(stop_token));
      } catch (const std::exception &exc) {
        app.logger().error("Reader thread failed: {}", exc.what());
        app.request_termination();
      } catch (...) {
        app.logger().error("Reader thread failed with unknown exception");
        app.request_termination();
      }

      node_queue.close();
      edge_queue.close();
      load_queue.close();
    });
    for (const auto& writer : writers) {
      threads.emplace_back(
        [&app, writer](const std::stop_token &stop_token) -> void {
          try {
            writer->run(stop_token);
          } catch (const std::exception &exc) {
            app.logger().error("Writer thread failed: {}", exc.what());
            app.request_termination();
          } catch (...) {
            app.logger().error("Writer thread failed with unknown exception");
            app.request_termination();
          }
        });
    }
    threads.emplace_back(
        [&app, &wrapper, &solution_queue,
         &active_solver_threads](const std::stop_token &stop_token) -> void {
          try {
            wrapper.run(stop_token);
          } catch (const std::exception &exc) {
            app.logger().error("Solver thread failed: {}", exc.what());
            app.request_termination();
          } catch (...) {
            app.logger().error("Solver thread failed with unknown exception");
            app.request_termination();
          }

          if (active_solver_threads.fetch_sub(1) == 1) {
            solution_queue.close();
          }
        });

    for (int idx = 1; idx < solver_threads; ++idx) {
      auto secondary_wrapper = std::make_shared<SolverWrapper>(
          SolverWrapper::RuntimeQueues{.node = nullptr,
                                       .edge = nullptr,
                                       .load = &load_queue,
                                       .solution = &solution_queue},
          wrapper.get_solver(), m_facility_timings_filename,
          wrapper.get_cache(), wrapper.get_facility_profiles());
      secondary.push_back(secondary_wrapper);
      threads.emplace_back(
          [&app, &solution_queue, &active_solver_threads,
           secondary_wrapper](const std::stop_token &stop_token) -> void {
            try {
              secondary_wrapper->run(stop_token);
            } catch (const std::exception &exc) {
              app.logger().error("Solver thread failed: {}", exc.what());
              app.request_termination();
            } catch (...) {
              app.logger().error("Solver thread failed with unknown exception");
              app.request_termination();
            }

            if (active_solver_threads.fetch_sub(1) == 1) {
              solution_queue.close();
            }
          });
    }
  } catch (const std::exception &exc) {
    app.logger().error("MAIN: Error occurred: {}", exc.what());
    app.request_termination();
  } catch (...) {
    app.logger().error("MAIN: Error occurred");
    app.request_termination();
  }

  app.wait_for_termination_request();
  app.logger().information("Termination requested");
  for (auto &thread : threads) {
    thread.request_stop();
  }

  return 0;
}

auto Moirai::run(int argc, char **argv) -> int {
  auto &app = moirai::Application::instance();

  try {
    parse_options(argc, argv);
    if (m_help_requested) {
      display_help();
      return 0;
    }

    moirai::Application::install_signal_handlers();
    return run_pipeline();
  } catch (const std::exception &exc) {
    app.logger().error(exc.what());
    display_help();
    return 1;
  } catch (...) {
    app.logger().error("Unhandled error");
    display_help();
    return 1;
  }
}
