#ifndef MOIRAI_SOLVER_WRAPPER
#define MOIRAI_SOLVER_WRAPPER

#include "concurrentqueue.h"
#include "date_utils.hxx"
#include "solver.hxx"
#include "transportation.hxx"
#include <Poco/Runnable.h>
#include <Poco/URI.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

class SolverWrapper : public Poco::Runnable
{
private:
  std::shared_ptr<Solver> solver;
  moodycamel::ConcurrentQueue<std::string>* mLoadQueuePtr;
  moodycamel::ConcurrentQueue<std::string>* mSolutionQueuePtr;

#ifdef WITH_NODE_FILE
  std::filesystem::path nodeFile;
#else
  Poco::URI node_uri;
  std::string node_idx, node_user, node_pass;
#endif

#ifdef WITH_EDGE_FILE
  std::filesystem::path edgeFile;
#else
  Poco::URI edge_uri;
  std::string edge_auth;
#endif

public:
  std::atomic<bool> running;

  SolverWrapper(moodycamel::ConcurrentQueue<std::string>*,
                moodycamel::ConcurrentQueue<std::string>*,
                std::shared_ptr<Solver>);

  SolverWrapper(moodycamel::ConcurrentQueue<std::string>*,
                moodycamel::ConcurrentQueue<std::string>*
#ifdef WITH_NODE_FILE
                ,
                std::filesystem::path&
#else
                ,
                std::string&,
                std::string&,
                std::string&,
                std::string&
#endif
#ifdef WITH_EDGE_FILE
                ,
                std::filesystem::path&
#else
                ,
                std::string&,
                std::string&
#endif
  );

  void init_nodes();

  void init_custody();

  void init_edges();

  auto read_nodes(const std::filesystem::path&)
    -> std::vector<std::shared_ptr<TransportCenter>>;

  auto find_paths(const std::string&,
                  const std::string&,
                  const std::string&,
                  datetime,
                  datetime,
                  const std::vector<TransportationLoadAttributes>&) const
    -> nlohmann::json;

  void run() override;
};
#endif
