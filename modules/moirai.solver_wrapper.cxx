module;

#include "blocking_queue_fwd.hxx"
#include "concurrent_cache.hxx"

export module moirai.solver_wrapper;

export import std;
export import moirai.date_utils;
export import moirai.http;
export import moirai.search_document;
export import moirai.solver;
export import moirai.transportation;

export struct PathCacheEntry {
  std::vector<SearchPathLocation> locations;
  CLOCK first_distance{};
  CLOCK last_distance{};
  bool found{false};
};

export using PathCache = ConcurrentCache<PathCacheEntry>;

export struct PathCacheConfig {
  bool enabled{true};
  std::size_t max_entries{65'536};
  std::uint32_t bucket_minutes{1};
};

export class SolverWrapper {
public:
  using HttpGet = std::function<moirai::HttpResponse(
      const moirai::Uri&, const std::vector<std::string>&)>;

  struct RuntimeQueues {
    BlockingQueue<std::string>* node;
    BlockingQueue<std::string>* edge;
    BlockingQueue<std::string>* load;
    BlockingQueue<SearchDocument>* solution;
  };

  struct InitEndpoints {
    std::string node_uri;
    std::string node_token;
    std::string edge_uri;
    std::string edge_token;
  };

private:
  std::shared_ptr<Solver> m_solver;

  moirai::Uri m_node_init_uri;
  std::string m_node_init_auth_token;

  moirai::Uri m_edge_init_uri;
  std::string m_edge_init_auth_token;

  std::unordered_map<
      std::string,
      std::tuple<std::int16_t, std::int16_t, std::int16_t, std::int16_t,
                 TIME_OF_DAY>>
      m_facility_timings_map;

  std::unordered_map<std::string, std::vector<std::string>> m_facility_groups;

  BlockingQueue<std::string>* m_node_queue{nullptr};
  BlockingQueue<std::string>* m_edge_queue{nullptr};
  BlockingQueue<std::string>& m_load_queue;
  BlockingQueue<SearchDocument>& m_solution_queue;
  HttpGet m_http_get;
  std::shared_ptr<PathCache> m_path_cache;
  PathCacheConfig m_cache_config;

public:
  SolverWrapper(RuntimeQueues queues, const std::shared_ptr<Solver>& solver,
                const std::filesystem::path& center_timings_filename,
                std::shared_ptr<PathCache> cache = nullptr,
                HttpGet http_get = moirai::http_get);

  SolverWrapper(RuntimeQueues queues, InitEndpoints endpoints,
                const std::filesystem::path& center_timings_filename,
                HttpGet http_get = moirai::http_get);

  void init_timings(const std::filesystem::path& facility_timings_filename);

  void init_nodes(std::int16_t page = 1);

  void init_custody();

  void init_edges();

  auto read_vertices(const std::filesystem::path& path)
      -> std::vector<std::shared_ptr<TransportCenter>>;

  [[nodiscard]] auto get_solver() const -> std::shared_ptr<Solver>;
  [[nodiscard]] auto get_cache() const -> std::shared_ptr<PathCache>;

  auto find_paths(
      std::string bag, std::string bag_source, std::string bag_target,
      std::int32_t bag_start, CLOCK bag_end,
      std::vector<std::tuple<std::string, std::int32_t, std::string>>& packages)
      const -> SearchDocument;

  void run(const std::stop_token& stop_token);
};
