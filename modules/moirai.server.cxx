export module moirai.server;

export import std;
export import moirai.utils;

export class Moirai {
private:
  std::vector<std::string> m_broker_url;
  TopicMap m_topic_map;
  std::unordered_map<std::string, std::string> m_kafka_properties;
  std::uint16_t m_batch_size{100};
  std::uint16_t m_timeout{1000};

  std::string m_search_uri;
  std::string m_search_user;
  std::string m_search_pass;
  std::string m_search_index;
  std::uint16_t m_search_writer_threads{1};

  std::string m_facility_timings_filename;
  std::string m_facility_uri;
  std::string m_facility_token;

  std::string m_route_uri;
  std::string m_route_token;

  std::string m_scans_query_file;

  bool m_help_requested = false;
  bool m_local_mode = false;
  std::string m_command_name = "moirai";

  void display_help() const;
  void parse_options(int argc, char** argv);
  void validate_options() const;
  auto run_pipeline() -> int;

public:
  auto run(int argc, char** argv) -> int;
};
