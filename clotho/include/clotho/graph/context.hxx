#ifndef GRAPH_CONTEXT_HXX
#define GRAPH_CONTEXT_HXX

#include <clotho/graph/strategy.hxx>

namespace ambasta {

class SolverContext
{
private:
  Strategy* m_strategy;

public:
  SolverContext(Strategy* strategy = nullptr)
    : m_strategy(strategy)
  {}

  ~SolverContext() { delete this->m_strategy; }

  void set_strategy(Strategy* strategy)
  {
    delete this->m_strategy;
    this->m_strategy = strategy;
  }

  void business_logic() const
  {
    // business logic here
    // call solve
    // parse solved path
    // emit json?
  }
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
