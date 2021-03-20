#ifndef GRAPH_EARLIEST_HXX
#define GRAPH_EARLIEST_HXX
#include <clotho/graph/strategy.hxx>

namespace ambasta {
class ShortestPathSolver : public Strategy
{
public:
  void solve(std::string_view,
             std::string_view,
             const TIMESTAMP&,
             const bool) const override;

  void solve(std::string_view,
             std::string_view,
             const TIMESTAMP&,
             const bool,
             const std::pair<TIMESTAMP, LEVY>&) const override;
};

/*template<>
class Solver<Algorithm::SHORTEST> : public BaseSolver
{

}
};*/
}
#endif
