#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/property_map/function_property_map.hpp>
#include <clotho/graph/earliest.hxx>
#include <functional>

using namespace ambasta;

bool
ShortestPathSolver::compare(const COST& lhs, const COST& rhs) const
{
  return lhs.first <= rhs.first;
}

COST
ShortestPathSolver::combine(
  const COST& distance,
  const std::tuple<TIME_OF_DAY, MINUTES, LEVY>& cost) const
{
  MINUTES wait_time{ std::get<0>(cost) - TIME_OF_DAY(distance.first) };
  return std::make_pair(distance.first + wait_time + std::get<1>(cost),
                        distance.second + std::get<2>(cost));
}

void
ShortestPathSolver::solve(std::string_view,
                          std::string_view,
                          const TIMESTAMP&,
                          const bool,
                          const std::pair<TIMESTAMP, LEVY>&) const
{}

const std::tuple<TIME_OF_DAY, MINUTES, LEVY>&
ShortestPathSolver::weight(const EdgeDescriptor&) const
{

  const Route* route = (*m_graph)[ed].get();
  const Node* source = (*m_graph)[boost::source(ed, *m_graph)].get();
  const Node* target = (*m_graph)[boost::target(ed, *m_graph)].get();

  return std::make_tuple(route->departure<Algorithm::SHORTEST>(source),
                         route->duration(source, target),
                         route->levy());
  /*return boost::make_function_property_map<EdgeDescriptor>(
    [this](const EdgeDescriptor& ed) {
      const Route* route = (*m_graph)[ed].get();
      const Node* source = (*m_graph)[boost::source(ed, *m_graph)].get();
      const Node* target = (*m_graph)[boost::target(ed, *m_graph)].get();

      return std::make_tuple(route->departure<Algorithm::SHORTEST>(source),
                             route->duration(source, target),
                             route->levy());
    });*/
}

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
    std::bind(&ShortestPathSolver::weight, this));

  std::vector<COST> distances(boost::num_vertices(*m_graph));
  std::vector<EdgeDescriptor> predecessors(boost::num_vertices(*m_graph));

  auto recorder =
    boost::record_edge_predecessors(&predecessors[0], boost::on_edge_relaxed());
  boost::dijkstra_visitor<decltype(recorder)> visitor(recorder);

  boost::dijkstra_shortest_paths(
    *m_graph,
    u,
    boost::distance_map(&distances[0])
      .weight_map(weight_map)
      .distance_compare([](const COST& lhs, const COST& rhs) -> bool {
        return lhs.first <= rhs.first;
      })
      .distance_combine(
        [](const COST& distance,
           const std::tuple<TIME_OF_DAY, MINUTES, LEVY> cost) -> COST {
          MINUTES wait_time{ std::get<0>(cost) - TIME_OF_DAY(distance.first) };
          return std::make_pair(distance.first + wait_time + std::get<1>(cost),
                                distance.second + std::get<2>(cost));
        })
      .distance_zero(std::make_pair((MINUTES)start, 0))
      .distance_inf(
        std::make_pair(MINUTES::max(), std::numeric_limits<LEVY>::max()))
      .visitor(visitor));
}
