module;

#include "blocking_queue_fwd.hxx"

export module moirai.search_writer;

export import std;
export import moirai.http;
export import moirai.search_document;

export struct SearchIndexConfig {
  std::size_t shards{24};
  std::size_t replicas{1};
  std::string refresh_interval{"30s"};
  double shard_warning_ratio{1.5};
  double shard_critical_ratio{2.0};
  std::size_t bulk_max_records{1024};
  std::size_t bulk_max_bytes{8U * 1024U * 1024U};
  std::size_t bulk_max_retries{3};
  std::size_t log_sample_ids{5};
  std::chrono::seconds metrics_interval{30};
  bool bulk_gzip{true};
  int bulk_gzip_level{1};
  bool bulk_adaptive{true};
  std::size_t bulk_min_records{128};
  std::chrono::milliseconds bulk_target_latency{2000};
  bool audit_enabled{false};
  std::filesystem::path audit_dir;
  std::size_t audit_rotate_records{100'000};
  std::size_t audit_rotate_bytes{128U * 1024U * 1024U};
  std::chrono::seconds audit_rotate_interval{300};
  bool audit_kafka_enabled{false};
  std::string audit_kafka_brokers;
  std::string audit_kafka_topic;
  std::unordered_map<std::string, std::string> audit_kafka_properties;
  std::chrono::milliseconds audit_kafka_flush_timeout{30'000};
  std::size_t audit_kafka_queue_retries{3};
  bool audit_kafka_required{true};

  static auto from_environment() -> SearchIndexConfig;
};

export using SearchHttpRequest =
  std::function<moirai::HttpResponse(std::string_view,
                                     const moirai::Uri&,
                                     std::string_view,
                                     const std::vector<std::string>&)>;

export using AuditPublish =
  std::function<void(std::string_view, std::string_view)>;

export class SearchWriter {
private:
  moirai::Uri m_uri;
  const std::string m_username;
  const std::string m_password;
  const std::string m_search_index;
  const SearchIndexConfig m_index_config;
  BlockingQueue<SearchDocument>& m_solution_queue;
  SearchHttpRequest m_http_request;
  AuditPublish m_audit_publish;
  bool m_index_ready{false};
  bool m_shared_index_guard{false};

  [[nodiscard]] auto authorization_headers() const
      -> std::vector<std::string>;
  [[nodiscard]] auto json_headers() const -> std::vector<std::string>;
  [[nodiscard]] auto bulk_headers(bool compressed) const
      -> std::vector<std::string>;
  [[nodiscard]] auto create_index_body() const -> std::string;
  void create_index();
  auto update_index_mapping() -> bool;
  void validate_index_definition();
  void check_shard_balance();

public:
  SearchWriter(moirai::Uri uri, std::string search_user,
               std::string search_pass, std::string search_index,
               BlockingQueue<SearchDocument>* solution_queue);

  SearchWriter(moirai::Uri uri, std::string search_user,
               std::string search_pass, std::string search_index,
               BlockingQueue<SearchDocument>* solution_queue,
               SearchIndexConfig index_config,
               SearchHttpRequest http_request);

  SearchWriter(moirai::Uri uri, std::string search_user,
               std::string search_pass, std::string search_index,
               BlockingQueue<SearchDocument>* solution_queue,
               SearchIndexConfig index_config,
               SearchHttpRequest http_request,
               AuditPublish audit_publish);

  void ensure_index_ready();
  void run(const std::stop_token& stop_token);
};
