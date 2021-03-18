#ifndef GRAPH_EARLIEST_HXX
#define GRAPH_EARLIEST_HXX
#include "graph/solver.hxx"
#include "typedefs.hxx"

namespace ambasta {

template<typename V, typename E>
class Solver<V, E, Algorithm::SHORTEST> : public BaseSolver<V, E>
{
  void path(std::string_view,
            std::string_view,
            const TIMESTAMP&,
            const bool) const;
};

/* void
path(std::string_view u_label,
     std::string_view v_label,
     TIMESTAMP start,
     const bool restricted) const
{
  auto filter_edges = [this, &restricted](const EdgeDescriptor& ed) -> bool {
    std::shared_ptr<E> edge = m_graph[ed];
    return edge->restricted || restricted;
  };

  VertexDescriptor u = boost::vertex_by_label(u_label, m_graph);
  VertexDescriptor v = boost::vertex_by_label(v_label, m_graph);
  auto null_v = boost::graph_traits<_Graph>::null_vertex();

  if (u == null_v or v == null_v)
    return;

  auto weight_map = boost::make_function_property_map<EdgeDescriptor>(
    [this](EdgeDescriptor& ed) {
      auto source = boost::source(ed, m_graph);
      auto target = boost::target(ed, m_graph);
      auto edge = m_graph[ed];
      return std::make_pair(edge->departure<Direction::F>(source),
                            edge->duration<Direction::F>(source, target));
    });

  boost::filtered_graph<_Graph, decltype(filter_edges)> filtered_graph{
    m_graph, filter_edges
  };

  std::vector<TIMESTAMP_MINUTES> distances(boost::num_vertices(m_graph));
  std::vector<EdgeDescriptor> predecessors(boost::num_edges(m_graph));*/

/*typedef std::map<VertexDescriptor, EdgeDescriptor>
predecessors_edge_map_t; typedef
boost::associative_property_map<predecessors_edge_map_t>
  predecessors_edge_property_map_t;*/

/* auto recorder =
  boost::record_edge_predecessors(predecessors, boost::on_edge_relaxed());
boost::dijkstra_visitor<decltype(recorder)> visitor(recorder);

boost::dijkstra_shortest_paths(
  filtered_graph,
  u,
  boost::distance_map(&distances[0])
    .weight_map(weight_map)
    .distance_combine(
      [](const TIMESTAMP_MINUTES& distance,
         const std::pair<TIME_OF_DAY, MINUTES> cost) -> TIMESTAMP_MINUTES {
        MINUTES wait_time{ cost.first - TIME_OF_DAY(distance) };
        return distance + wait_time + cost.second;
      })
    .distance_zero((TIMESTAMP_MINUTES)start)
    .distance_inf(TIMESTAMP_MINUTES::max())
    .visitor(visitor));
}
*/
}
#endif
