#include <boost/graph/dijkstra_shortest_paths.hpp>
// #include <clotho/boost/graph/filtered_graph.hxx>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/visitors.hpp>
#include <boost/property_map/function_property_map.hpp>
#include <boost/property_map/property_map.hpp>
#include <clotho/graph/strategy.hxx>
#include <memory>

using namespace ambasta;

std::pair<const std::shared_ptr<Node>, bool>
Strategy::vertex(std::string_view label) const
{
  auto vertex = boost::vertex_by_label(label, *m_graph);

  if (vertex == boost::graph_traits<Graph>::null_vertex()) {
    return { nullptr, false };
  }
  return { (*m_graph)[vertex], true };
}

std::pair<const std::shared_ptr<Node>, bool>
Strategy::vertex(const VertexDescriptor& v) const
{
  return { (*m_graph)[v], true };
}

std::pair<const std::shared_ptr<Route>, bool>
Strategy::edge(const EdgeDescriptor& e) const
{
  return { (*m_graph)[e], true };
}

std::pair<const VertexDescriptor, bool>
Strategy::add_vertex(std::shared_ptr<Node> vp)
{
  const VertexDescriptor vertex = boost::add_vertex(vp->label(), vp, *m_graph);

  if (vertex == boost::graph_traits<Graph>::null_vertex())
    return { vertex, false };
  return { vertex, true };
}

std::pair<const EdgeDescriptor, bool>
Strategy::add_edge(std::string_view u_label,
                   std::string_view v_label,
                   std::shared_ptr<Route> ep)
{
  return boost::add_edge_by_label(u_label, v_label, ep, *m_graph);
}

std::pair<const EdgeDescriptor, bool>
Strategy::add_edge(std::shared_ptr<Node> u,
                   std::shared_ptr<Node> v,
                   std::shared_ptr<Route> ep)
{
  return add_edge(u->label(), v->label(), ep);
}

Strategy::Strategy()
{
  m_graph = std::make_shared<Graph>();
}

Strategy::Strategy(std::shared_ptr<Graph> graph)
  : m_graph(graph)
{}

Strategy::~Strategy()
{
  m_graph.reset();
}

/*
void
Strategy::solve(std::string_view u_label,
                std::string_view v_label,
                const TIMESTAMP& start,
                const bool restricted,
                const std::pair<TIMESTAMP, LEVY>& limits) const
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
    [this](const EdgeDescriptor& ed) -> std::tuple<TIME_OF_DAY, MINUTES, LEVY> {
      return weight(ed);
    });

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
      .distance_compare([this](const COST& lhs, const COST& rhs) -> bool {
        return compare(lhs, rhs);
      })
      .distance_combine(
        [this](const COST& distance,
               const std::tuple<TIME_OF_DAY, MINUTES, LEVY>& cost) -> COST {
          return combine(distance, cost);
        })
      .distance_zero(zero(start))
      .distance_inf(inf())
      .visitor(visitor));
}
*/

void
Strategy::solve(std::string_view u_label,
                std::string_view v_label,
                const TIMESTAMP& start,
                const bool restricted) const
{
  VertexDescriptor u = boost::vertex_by_label(u_label, *m_graph);
  VertexDescriptor v = boost::vertex_by_label(v_label, *m_graph);
  auto null_v = boost::graph_traits<Graph>::null_vertex();

  if (u == null_v or v == null_v)
    return;

  auto filter_edges = [this, &restricted](const EdgeDescriptor& ed) -> bool {
    std::shared_ptr<Route> edge = (*m_graph)[ed];
    return edge->unrestricted() || restricted;
  };

  boost::filtered_graph<Graph,
                        std::function<bool(const EdgeDescriptor&)>,
                        boost::keep_all>
    filtered_graph(*m_graph, filter_edges, boost::keep_all{});

  auto weight_map = boost::make_function_property_map<EdgeDescriptor>(
    [this](const EdgeDescriptor& ed) -> std::tuple<TIME_OF_DAY, MINUTES, LEVY> {
      return weight(ed);
    });

  std::vector<COST> distances(boost::num_vertices(filtered_graph));
  std::vector<EdgeDescriptor> predecessors(boost::num_vertices(filtered_graph));

  auto recorder =
    boost::record_edge_predecessors(&predecessors[0], boost::on_edge_relaxed());
  boost::dijkstra_visitor<decltype(recorder)> visitor(recorder);

  boost::dijkstra_shortest_paths(
    filtered_graph,
    u,
    boost::distance_map(&distances[0])
      .weight_map(weight_map)
      .distance_compare([this](const COST& lhs, const COST& rhs) -> bool {
        return compare(lhs, rhs);
      })
      .distance_combine(
        [this](const COST& distance,
               const std::tuple<TIME_OF_DAY, MINUTES, LEVY>& cost) -> COST {
          return combine(distance, cost);
        })
      .distance_zero(zero(start))
      .distance_inf(inf())
      .visitor(visitor));
}

/*
template<>
bool
Strategy::compare<Algorithm::SHORTEST>(const COST& lhs, const COST& rhs)
{
  return lhs.first <= rhs.first;
}

template<>
bool
Strategy::compare<Algorithm::SHORTEST_CONSTRAINED>(const COST& lhs,
                                                   const COST& rhs)
{
  return (lhs.second <= rhs.second and lhs.first <= rhs.first) or false;
}

template<>
bool
Strategy::compare<Algorithm::INVERSE_SHORTEST>(const COST& lhs, const COST& rhs)
{
  return lhs.first >= rhs.first;
}

template<>
COST
Strategy::combine<Algorithm::SHORTEST>(
  const COST& distance,
  const std::tuple<TIME_OF_DAY, MINUTES, LEVY>& cost)
{
  MINUTES wait_time{ std::get<0>(cost) - TIME_OF_DAY(distance.first) };
  return std::make_pair(distance.first + wait_time + std::get<1>(cost),
                        distance.second + std::get<2>(cost));
}

template<>
COST
Strategy::combine<Algorithm::SHORTEST_CONSTRAINED>(
  const COST& distance,
  const std::tuple<TIME_OF_DAY, MINUTES, LEVY>& cost)
{
  return combine<Algorithm::SHORTEST>(distance, cost);
}

template<>
COST
Strategy::combine<Algorithm::INVERSE_SHORTEST>(
  const COST& distance,
  const std::tuple<TIME_OF_DAY, MINUTES, LEVY>& cost)
{}
*/
