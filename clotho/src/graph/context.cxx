#include <clotho/graph/context.hxx>

namespace ambasta {
SolverContext::SolverContext(Strategy* strategy)
  : m_strategy(strategy)
{}

SolverContext::~SolverContext()
{
  delete this->m_strategy;
}

void
SolverContext::set_strategy(Strategy* strategy)
{
  delete this->m_strategy;
  this->m_strategy = strategy;
}

void
SolverContext::business_logic() const
{
  // business logic here
  // call solve
  // parse solved path
  // emit json?
}
}
