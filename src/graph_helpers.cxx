#include "graph_helpers.hxx"

template<>
CLOCK
Combine<PathTraversalMode::FORWARD>::operator()(CLOCK start,
                                                DURATION delta) const
{
  return start + delta;
}

template<>
CLOCK
Combine<PathTraversalMode::REVERSE>::operator()(CLOCK start,
                                                DURATION delta) const
{
  return start - delta;
}

template<>
bool
Compare<PathTraversalMode::FORWARD>::operator()(CLOCK lhs, CLOCK rhs) const
{
  return lhs < rhs;
}

template<>
bool
Compare<PathTraversalMode::REVERSE>::operator()(CLOCK lhs, CLOCK rhs) const
{
  return rhs < lhs;
}
