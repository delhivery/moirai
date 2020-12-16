#include "graph_helpers.hxx"
#include <boost/graph/adjacency_list.hpp>

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
