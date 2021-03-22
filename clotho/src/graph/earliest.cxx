#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/property_map/function_property_map.hpp>
#include <clotho/graph/earliest.hxx>

namespace ambasta {

void
ShortestPathSolver::solve(std::string_view,
                          std::string_view,
                          const TIMESTAMP&,
                          const bool,
                          const std::pair<TIMESTAMP, LEVY>&) const
{}

void
ShortestPathSolver::solve(std::string_view u_label,
                          std::string_view v_label,
                          const TIMESTAMP& start,
                          const bool restricted) const
{
  auto filter_edges = [this, &restricted](const EdgeDescriptor& ed) -> bool {
    std::shared_ptr<Route> edge = (*m_graph)[ed];
    return edge->unrestricted() || restricted;
  };

  VertexDescriptor u = boost::vertex_by_label(u_label, *m_graph);
  VertexDescriptor v = boost::vertex_by_label(v_label, *m_graph);
  auto null_v = boost::graph_traits<Graph>::null_vertex();

  if (u == null_v or v == null_v)
    return;

  auto weight_map = boost::make_function_property_map<EdgeDescriptor>(
    [this](const EdgeDescriptor& ed) {
      auto source = boost::source(ed, *m_graph);
      auto target = boost::target(ed, *m_graph);
      auto edge = (*m_graph)[ed];
      return std::make_pair(
        edge->departure<Algorithm::SHORTEST>((*m_graph)[source]),
        edge->duration((*m_graph)[source], (*m_graph)[target]));
    });

  std::vector<MINUTES> distances(boost::num_vertices(*m_graph));
  std::vector<EdgeDescriptor> predecessors(boost::num_vertices(*m_graph));

  auto recorder =
    boost::record_edge_predecessors(&predecessors[0], boost::on_edge_relaxed());
  boost::dijkstra_visitor<decltype(recorder)> visitor(recorder);

  boost::dijkstra_shortest_paths(
    *m_graph,
    u,
    boost::distance_map(&distances[0])
      .weight_map(weight_map)
      .distance_combine(
        [](const MINUTES& distance,
           const std::pair<TIME_OF_DAY, MINUTES> cost) -> MINUTES {
          MINUTES wait_time{ cost.first - TIME_OF_DAY(distance) };
          return distance + wait_time + cost.second;
        })
      .distance_zero((MINUTES)start)
      .distance_inf(MINUTES::max())
      .visitor(visitor));
}

};
