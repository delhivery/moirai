#ifndef GRAPH_EARLIEST_HXX
#define GRAPH_EARLIEST_HXX
#include <clotho/graph/strategy.hxx>

namespace ambasta {
class ShortestPathSolver : public Strategy
{
public:
  COST zero(const TIMESTAMP = TIMESTAMP::min(),
            const LEVY = std::numeric_limits<LEVY>::min()) const override;

  COST inf(const TIMESTAMP = TIMESTAMP::max(),
           const LEVY = std::numeric_limits<LEVY>::max()) const override;

  bool compare(const COST&, const COST&) const override;

  COST combine(const COST&,
               const std::tuple<TIME_OF_DAY, MINUTES, LEVY>&) const override;

  const std::tuple<TIME_OF_DAY, MINUTES, LEVY> weight(
    const EdgeDescriptor&) const override;
};

/*template<>
class Solver<Algorithm::SHORTEST> : public BaseSolver
{

}
};*/
}
#endif
