#include <chrono>  // for operator<=>, duration, time_point
#include <compare> // for operator<, strong_ordering

#include "graph_helpers.hxx"
#include "edge_cost_attributes.hxx"

template<>
bool
Compare<PathTraversalMode::FORWARD>::operator()(datetime lhs, datetime rhs) const
{
  return lhs < rhs;
}

template<>
bool
Compare<PathTraversalMode::REVERSE>::operator()(datetime lhs, datetime rhs) const
{
  return rhs < lhs;
}
