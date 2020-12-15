#ifndef moirai_graph_helpers
#define moirai_graph_helpers
#include "transportation.hxx"
#include "utils.hxx"
#include <boost/graph/graph_traits.hpp>
#include <chrono>
#include <type_traits>

template<class G, VehicleType V>
struct FilterByType
{
  G* graph;

  bool operator()(typename boost::graph_traits<G>::edge_descriptor edge)
  {
    auto edge_props = graph[edge];
    return edge_props.vehicle == V;
  }
};

template<PathTraversalMode>
struct Combine
{
  CLOCK operator()(CLOCK, DURATION) const;
};

template<PathTraversalMode>
struct Compare
{
  bool operator()(CLOCK, CLOCK) const;
};
#endif
