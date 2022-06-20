#ifndef MOIRAI_SOLVER_WRAPPER
#define MOIRAI_SOLVER_WRAPPER

#include "consumer.hxx"
#include "producer.hxx"
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

class SolverWrapper
  : public Consumer
  , public Producer
{
  using consumer_base_t = ::Consumer;
  using producer_base_t = ::Producer;

private:
  bool mRunning;

  Solver* mSolverPtr;

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
  SolverWrapper(Solver*,
                producer_base_t::queue_t*,
                consumer_base_t::queue_t*,
                size_t);

  SolverWrapper(const SolverWrapper&);

  SolverWrapper(Solver*,
                producer_base_t::queue_t*,
                consumer_base_t::queue_t*,
                size_t
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

  auto logger() const -> Poco::Logger& override;

  void stop(bool);

  void init_nodes();

  void init_edges();

  void load_edges(const nlohmann::json&);

  auto find_paths(const std::string&,
                  const std::string&,
                  const std::string&,
                  datetime,
                  datetime,
                  auto&) const -> nlohmann::json;

  void run() override final;
};
#endif
