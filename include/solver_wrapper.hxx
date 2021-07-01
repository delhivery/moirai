#ifndef MOIRAI_SOLVER_WRAPPER
#define MOIRAI_SOLVER_WRAPPER

#include "concurrentqueue.h"
#include "solver.hxx"
#include "transportation.hxx"
#include <Poco/Runnable.h>
#include <Poco/URI.h>
#include <filesystem>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

class SolverWrapper : public Poco::Runnable
{
private:
  std::shared_ptr<Solver> solver;

  Poco::URI node_init_uri;
  std::string node_init_auth_token;

  Poco::URI edge_init_uri;
  std::string edge_init_auth_token;

  std::unordered_map<std::string,
                     std::tuple<int16_t, int16_t, int16_t, int16_t>>
    facility_timings_map;

  std::unordered_map<std::string, std::vector<std::string>> facility_groups;

  moodycamel::ConcurrentQueue<std::string>* node_queue;
  moodycamel::ConcurrentQueue<std::string>* edge_queue;
  moodycamel::ConcurrentQueue<std::string>* load_queue;
  moodycamel::ConcurrentQueue<std::string>* solution_queue;

public:
  std::atomic<bool> running;

  SolverWrapper(moodycamel::ConcurrentQueue<std::string>*,
                moodycamel::ConcurrentQueue<std::string>*,
                const std::shared_ptr<Solver>);

  SolverWrapper(moodycamel::ConcurrentQueue<std::string>*,
                moodycamel::ConcurrentQueue<std::string>*,
                moodycamel::ConcurrentQueue<std::string>*,
                moodycamel::ConcurrentQueue<std::string>*,
                const std::string&,
                const std::string&,
                const std::string&,
                const std::string&,
                const std::filesystem::path&);

  void init_timings(const std::filesystem::path&);

  void init_nodes(int16_t = 1);

  void init_custody();

  void init_edges();

  std::vector<std::shared_ptr<TransportCenter>> read_vertices(
    const std::filesystem::path&);

  const std::shared_ptr<Solver> get_solver() const;

  auto find_paths(
    std::string bag,
    std::string bag_source,
    std::string bag_target,
    int32_t bag_start,
    CLOCK bag_end,
    std::vector<std::tuple<std::string, int32_t, std::string>>&) const;

  virtual void run();
};
#endif
