#ifndef moirai_graph_helpers
#define moirai_graph_helpers

#include "date_utils.hxx"
#include "transportation.hxx"
#include <boost/graph/graph_traits.hpp> // for graph_traits
#include <cstring>                      // for memset

static const char emptyChar = 0xFF;

template<class G, VehicleType V>
struct FilterByVehicleType
{
  const G* graph;

  FilterByVehicleType() = default;

  FilterByVehicleType(const G* graph)
    : graph(graph)
  {
  }

  auto operator()(
    const typename boost::graph_traits<G>::edge_descriptor& edge) const -> bool
  {
    auto edgeProps = (*graph)[edge];
    return edgeProps->vehicle() <= V;
  }
};

template<typename E>
static auto
null_edge() -> E
{
  E nullEdge;
  memset((char*)&nullEdge, emptyChar, sizeof(E));
  return nullEdge;
}
#endif
