#ifndef MOIRAI_DIJKSTRA
#define MOIRAI_DIJKSTRA

#include "date_utils.hxx"
#include "graph_helpers.hxx"
#include <algorithm>
#include <boost/graph/graph_archetypes.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/pending/indirect_cmp.hpp>
#include <boost/tuple/tuple.hpp>
#include <map>
#include <queue>
#include <set>

namespace moirai {
template<typename Graph,
         typename PredecessorVertexMap,
         typename PredecessorEdgeMap,
         typename DistanceMap,
         typename VertexIndexMap,
         typename Cost,
         typename DistanceCompare,
         typename DistanceWeightCombine>
void
dijkstra_shortest_paths(
  const Graph& graph,
  typename boost::graph_traits<Graph>::vertex_descriptor source,
  PredecessorVertexMap predecessor_vertex_map,
  PredecessorEdgeMap predecessor_edge_map,
  DistanceMap distance_map,
  VertexIndexMap vertex_index_map,
  DistanceCompare distance_compare,
  DistanceWeightCombine distance_weight_combine,
  Cost inf,
  Cost zero)
{
  typedef typename boost::graph_traits<Graph>::vertex_descriptor Vertex;
  typedef typename boost::graph_traits<Graph>::edge_descriptor Edge;
  typename boost::graph_traits<Graph>::vertex_iterator vi, vi_end;

  for (boost::tie(vi, vi_end) = boost::vertices(graph); vi != vi_end; ++vi) {
    Vertex v = *vi;
    boost::put(distance_map, *vi, inf);
    boost::put(predecessor_vertex_map, *vi, *vi);
    boost::put(predecessor_edge_map, *vi, null_edge<Edge>());
  }

  boost::put(distance_map, source, zero);

  auto comparator = [&distance_map, &distance_compare](Vertex lhs,
                                                       Vertex rhs) -> bool {
    distance_compare(boost::get(distance_map, lhs),
                     boost::get(distance_map, rhs));
  };

  std::set<Vertex> priority_queue;
  std::set<Vertex> visited;
  priority_queue.insert(source);

  while (!priority_queue.empty()) {
    std::make_heap(priority_queue.begin(), priority_queue.end(), comparator);

    Vertex current = *priority_queue.begin();
    priority_queue.erase(priority_queue.begin());

    Cost distance_current = boost::get(distance_map, current);

    if (distance_current == inf) {
      return;
    }

    assert(
      ("Cyclic loop while running shortest paths", visited.contains(current)));

    for (auto [ei, ei_end] = boost::out_edges(current, graph); ei != ei_end;
         ++ei) {

      auto edge = graph[*ei];
      assert(("Edge weights cannot be negative",
              distance_compare(distance_weight_combine(zero, edge.weight(zero)),
                               zero)));

      Vertex target = boost::target(*ei, graph);
      Cost distance_target = boost::get(distance_map, target);

      bool is_neighbor_undiscovered = !distance_compare(distance_target, inf);

      Cost updated_distance = distance_weight_combine(
        distance_current, edge.weight(distance_current));

      if (distance_compare(updated_distance, distance_target)) {
        boost::put(distance_map, target, updated_distance);
        boost::put(predecessor_vertex_map, target, source);
        boost::put(predecessor_edge_map, target, *ei);
        priority_queue.insert(target);
      }
    }

    visited.insert(current);
  }
}
} // namespace moirai
#endif
