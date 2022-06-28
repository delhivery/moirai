#ifndef moirai_graph_helpers
#define moirai_graph_helpers

#include "transport.hxx"
#include <boost/graph/graph_traits.hpp>
#include <cstring>

static const char EMPTY_CHAR = 0xFF;

template <class G, Vehicle V> struct FilterByVehicle {
  using EdgeDescriptor = typename boost::graph_traits<G>::edge_descriptor;

  const G *graph;

  FilterByVehicle() = default;

  FilterByVehicle(const G *graph) : graph(graph) {}

  auto operator()(const EdgeDescriptor &edge) const -> bool {
    auto edgeProps = (*graph)[edge];
    return edgeProps->vehicle() <= V;
  }
};

template <typename E> static auto null_edge() -> E {
  E nullEdge;
  memset((char *)&nullEdge, EMPTY_CHAR, sizeof(E));
  return nullEdge;
}
#endif
