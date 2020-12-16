#include <chrono>  // for operator<=>, duration, time_point
#include <compare> // for operator<, strong_ordering

#include "graph_helpers.hxx"

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
