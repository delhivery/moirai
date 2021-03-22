#ifndef GRAPH_CONTEXT_HXX
#define GRAPH_CONTEXT_HXX

#include <clotho/graph/strategy.hxx>

namespace ambasta {

class SolverContext
{
private:
  Strategy* m_strategy;

public:
  SolverContext(Strategy* = nullptr);

  ~SolverContext();

  void set_strategy(Strategy*);

  void business_logic() const;
};

void
Client();
/*{
  SolverContext* context = new SolverContext(new ShortestPathSolver());
  context->business_logic();
  context->set_strategy(new CriticalPathSolver());
  context->business_logic();
  delete context;
}*/
}

#endif
