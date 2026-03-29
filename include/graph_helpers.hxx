#pragma once

#include <cstring> // for memset

#include <boost/graph/graph_traits.hpp> // for graph_traits

#include "date_utils.hxx"     // for CLOCK
#include "transportation.hxx" // for PathTraversalMode, VehicleType

inline constexpr unsigned char NULL_EDGE_FILL_BYTE = 0xFF;

template <class G, VehicleType V> struct FilterByVehicleType {
  const G *graph;

  FilterByVehicleType() = default;

  FilterByVehicleType(const G *graph) : graph(graph) {}

  auto operator()(

      const boost::graph_traits<G>::edge_descriptor &edge) const -> bool {
    auto edge_props = (*graph)[edge];
    return edge_props->vehicle <= V;
  }
};

template <PathTraversalMode> struct Compare {
  auto operator()(CLOCK, CLOCK) const -> bool;
};

template <typename E> static auto null_edge() -> E {
  E null_e;
  memset(reinterpret_cast<char *>(&null_e), NULL_EDGE_FILL_BYTE, sizeof(E));
  return null_e;
}
