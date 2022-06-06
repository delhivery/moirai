#ifndef MOIRAI_SOLVER_WRAPPER
#define MOIRAI_SOLVER_WRAPPER

#include "concurrentqueue.h"
#include "date_utils.hxx"
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

#ifdef WITH_NODE_FILE
  std::filesystem::path node_file;
#else
  Poco::URI node_uri;
  std::string node_idx, node_user, node_pass;
#endif

#ifdef WITH_EDGE_FILE
  std::filesystem::path edge_file;
#else
  Poco::URI edge_uri;
  std::string edge_auth;
#endif

  std::unordered_map<std::string, std::vector<Node<Graph>>> colocated_nodes;

  moodycamel::ConcurrentQueue<std::string>* load_queue;
  moodycamel::ConcurrentQueue<std::string>* solution_queue;

public:
  std::atomic<bool> running;

  SolverWrapper(moodycamel::ConcurrentQueue<std::string>*,
                moodycamel::ConcurrentQueue<std::string>*,
                const std::shared_ptr<Solver>,
                const std::filesystem::path&);

  SolverWrapper(moodycamel::ConcurrentQueue<std::string>*,
                moodycamel::ConcurrentQueue<std::string>*
#ifdef WITH_NODE_FILE
                ,
                const std::filesystem::path&
#else
                ,
                const std::string&,
                const std::string&,
                const std::string&,
                const std::string&
#endif
#ifdef WITH_EDGE_FILE
                ,
                const std::filesystem::path&
#else
                ,
                const std::string&,
                const std::string&
#endif
  );

  void init_nodes();

  void init_custody();

  void init_edges();

  std::vector<std::shared_ptr<TransportCenter>> read_nodes(
    const std::filesystem::path&);

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
