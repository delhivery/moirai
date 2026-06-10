#pragma once

#include "blocking_queue.hxx"
#include "date_utils.hxx"
#include "http.hxx"
#include "solver.hxx"
#include "transportation.hxx"
#include <filesystem>
#include <functional>
#include <nlohmann/json_fwd.hpp>
#include <stop_token>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

class SolverWrapper {
public:
  using HttpGet = std::function<moirai::HttpResponse(
      const moirai::Uri &, const std::vector<std::string> &)>;

  struct RuntimeQueues {
    BlockingQueue<std::string> *node;
    BlockingQueue<std::string> *edge;
    BlockingQueue<std::string> *load;
    BlockingQueue<std::string> *solution;
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
      std::string, std::tuple<int16_t, int16_t, int16_t, int16_t, TIME_OF_DAY>>
      m_facility_timings_map;

  std::unordered_map<std::string, std::vector<std::string>> m_facility_groups;

  BlockingQueue<std::string> *m_node_queue{nullptr};
  BlockingQueue<std::string> *m_edge_queue{nullptr};
  BlockingQueue<std::string> &m_load_queue;
  BlockingQueue<std::string> &m_solution_queue;
  HttpGet m_http_get;

public:
  SolverWrapper(RuntimeQueues queues, const std::shared_ptr<Solver> &solver,
                const std::filesystem::path &center_timings_filename,
                HttpGet http_get = moirai::http_get);

  SolverWrapper(RuntimeQueues queues, InitEndpoints endpoints,
                const std::filesystem::path &center_timings_filename,
                HttpGet http_get = moirai::http_get);

  void init_timings(const std::filesystem::path &facility_timings_filename);

  void init_nodes(int16_t page = 1);

  void init_custody();

  void init_edges();

  auto read_vertices(const std::filesystem::path &path)
      -> std::vector<std::shared_ptr<TransportCenter>>;

  [[nodiscard]] auto get_solver() const -> std::shared_ptr<Solver>;

  auto find_paths(std::string bag, std::string bag_source,
                  std::string bag_target, int32_t bag_start, CLOCK bag_end,
                  std::vector<std::tuple<std::string, int32_t, std::string>>
                      &packages) const -> nlohmann::json;

  void run(const std::stop_token &stop_token);
};
