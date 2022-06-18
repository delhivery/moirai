#ifndef MOIRAI_SOLVER_WRAPPER
#define MOIRAI_SOLVER_WRAPPER

#include <Poco/Logger.h>
#ifndef JSON_HAS_CPP_20
#define JSON_HAS_CPP_20
#endif

#ifndef JSON_HAS_RANGES
#define JSON_HAS_RANGES 1
#endif

#include "concurrentqueue.h"
#include "runnable.hxx"
#include "solver.hxx"
#include <Poco/Runnable.h>
#include <Poco/URI.h>
#include <filesystem>
#include <nlohmann/json.hpp>

class SolverWrapper : public Runnable
{
private:
  // Poco::Logger& mLogger = Poco::Logger::get("solver-wrapper");
  std::shared_ptr<Solver> mSolverPtr;
  moodycamel::ConcurrentQueue<std::string>* mLoadQueuePtr;
  moodycamel::ConcurrentQueue<nlohmann::json>* mSolutionQueuePtr;

#ifdef WITH_NODE_FILE
  std::filesystem::path mNodeFile;
#else
  Poco::URI node_uri;
  std::string node_idx, node_user, node_pass;
#endif

#ifdef WITH_EDGE_FILE
  std::filesystem::path mEdgeFile;
#else
  Poco::URI edge_uri;
  std::string edge_auth;
#endif

public:
  SolverWrapper(moodycamel::ConcurrentQueue<std::string>*,
                moodycamel::ConcurrentQueue<std::string>*,
                std::shared_ptr<Solver>);

  SolverWrapper(const SolverWrapper&);

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

  void init_edges();

  void load_edges(const nlohmann::json&);

  auto read_nodes(const std::filesystem::path&)
    -> std::vector<std::shared_ptr<TransportCenter>>;

  auto find_paths(const std::string&,
                  const std::string&,
                  const std::string&,
                  datetime,
                  datetime,
                  auto&) const -> nlohmann::json;

  void run() override;
};
#endif
