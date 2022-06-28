#ifndef MOIRAI_SOLVER_WRAPPER
#define MOIRAI_SOLVER_WRAPPER

#include "consumer.hxx"
#include "producer.hxx"
#include "runnable.hxx"
#include "solver.hxx"
#include <Poco/Logger.h>
#include <Poco/Runnable.h>
#include <Poco/URI.h>
#include <concurrentqueue/concurrentqueue.h>
#include <filesystem>
#include <nlohmann/json.hpp>

class Wrapper : public Consumer, public Producer {
  using consumer_base_t = ::Consumer;
  using producer_base_t = ::Producer;

private:
  bool mRunning;

  Solver *mSolverPtr;

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
  Wrapper(Solver *, producer_base_t::queue_t *, consumer_base_t::queue_t *,
          size_t);

  Wrapper(const Wrapper &);

  Wrapper(Solver *, producer_base_t::queue_t *, consumer_base_t::queue_t *,
          size_t
#ifdef WITH_NODE_FILE
          ,
          std::filesystem::path &
#else
          ,
          std::string &, std::string &, std::string &, std::string &
#endif
#ifdef WITH_EDGE_FILE
          ,
          std::filesystem::path &
#else
          ,
          std::string &, std::string &
#endif
  );

  auto logger() const -> Poco::Logger & override;

  void stop(bool);

  void init_nodes();

  void init_edges();

  void load_edges(const nlohmann::json &);

  auto find_paths(const std::string &, const std::string &, const std::string &,
                  datetime, datetime, auto &) const -> nlohmann::json;

  void run() override final;
};
#endif
