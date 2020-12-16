#ifndef moirai_graph_helpers
#define moirai_graph_helpers
#include "date_utils.hxx"
#include "transportation.hxx"
#include <boost/graph/graph_traits.hpp>
#include <chrono>
#include <cstring>
#include <type_traits>

template<class G, VehicleType V>
struct FilterByVehicleType
{
  G* graph;

  FilterByVehicleType() = default;

  FilterByVehicleType(const G* graph)
    : graph(graph)
  {}

  bool operator()(typename boost::graph_traits<G>::edge_descriptor edge)
  {
    auto edge_props = graph[edge];
    return edge_props.vehicle >= V;
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
  memset((char*)null_e, 0xFF, sizeof(E));
  return null_e;
}
#endif
