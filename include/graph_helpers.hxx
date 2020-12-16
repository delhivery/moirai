#ifndef moirai_graph_helpers
#define moirai_graph_helpers

#include <cstring> // for memset

#include <boost/graph/graph_traits.hpp> // for graph_traits

#include "date_utils.hxx"     // for CLOCK
#include "transportation.hxx" // for PathTraversalMode, VehicleType

template<class G, VehicleType V>
struct FilterByVehicleType
{
  const G* graph;

  FilterByVehicleType() = default;

  FilterByVehicleType(const G* graph)
    : graph(graph)
  {}

  bool operator()(const typename boost::graph_traits<G>::edge_descriptor& edge)
  {
    auto edge_props = graph[edge];
    return edge_props.vehicle <= V;
  }
};

template<PathTraversalMode>
struct Compare
{
  bool operator()(CLOCK, CLOCK) const;
};

template<typename E>
static E
null_edge()
{
  E null_e;
  memset((char*)&null_e, 0xFF, sizeof(E));
  return null_e;
}
#endif
