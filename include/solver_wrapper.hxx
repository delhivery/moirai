#ifndef MOIRAI_SOLVER_WRAPPER
#define MOIRAI_SOLVER_WRAPPER

#include "concurrentqueue.h"
#include "solver.hxx"
#include "transportation.hxx"
#include <Poco/Runnable.h>
#include <filesystem>
#include <string>
#include <vector>

class SolverWrapper : public Poco::Runnable
{
private:
  Solver solver;
  moodycamel::ConcurrentQueue<std::string>* load_queue;
  moodycamel::ConcurrentQueue<std::string>* solution_queue;

public:
  SolverWrapper(moodycamel::ConcurrentQueue<std::string>*,
                moodycamel::ConcurrentQueue<std::string>*);

  std::vector<std::shared_ptr<TransportCenter>> read_vertices(
    const std::filesystem::path&);

  std::vector<
    std::tuple<std::string, std::string, std::shared_ptr<TransportEdge>>>
  read_connections(const std::filesystem::path&);

  auto find_paths(std::string bag,
                  std::string bag_source,
                  std::string bag_target,
                  int32_t bag_start,
                  std::vector<std::tuple<std::string, int32_t>>&);

  virtual void run();
};
#endif
