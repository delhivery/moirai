#include "graph_helpers.hxx"

template <>
auto Compare<PathTraversalMode::FORWARD>::operator()(CLOCK lhs, CLOCK rhs) const
    -> bool {
  return lhs < rhs;
}

template <>
auto Compare<PathTraversalMode::REVERSE>::operator()(CLOCK lhs, CLOCK rhs) const
    -> bool {
  return rhs < lhs;
}
