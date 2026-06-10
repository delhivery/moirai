#include "solver.hxx"
#include "date_utils.hxx"                        // for CLOCK
#include "graph_helpers.hxx"                     // for FilterByVehicleType
#include "transportation.hxx"                    // for VehicleType, AIR
#include <boost/graph/detail/adjacency_list.hpp> // for get, num_vertices
#include <boost/graph/detail/edge.hpp>           // for operator!=, operator==
#include <boost/graph/filtered_graph.hpp>        // for filtered_graph
#include <boost/graph/reverse_graph.hpp>         // for get, make_reverse_g...
#include <boost/iterator/iterator_facade.hpp>    // for operator!=, operator++
#include <numeric>
#include <string> // for string

auto Solver::add_node(const std::string &node_code_or_name) const
    -> std::pair<Node<Graph>, bool> {
  if (m_vertex_by_name.contains(node_code_or_name)) {
    return {m_vertex_by_name.at(node_code_or_name), true};
  }
  return {Graph::null_vertex(), false};
}

auto Solver::add_node(const std::shared_ptr<TransportCenter> &center)
    -> std::pair<Node<Graph>, bool> {
  auto created = add_node(center->code);

  if (created.second) {
    return created;
  }
  Node<Graph> node = boost::add_vertex(center, m_graph);
  m_vertex_by_name[center->code] = node;
  return {node, true};
}

auto Solver::get_node(const Node<Graph> node) const
    -> std::shared_ptr<TransportCenter> {
  return m_graph[node];
}

auto Solver::add_edge(const std::string &edge_code) const
    -> std::pair<Edge<Graph>, bool> {
  if (m_edge_by_name.contains(edge_code)) {
    return {m_edge_by_name.at(edge_code), true};
  }
  return {null_edge<Edge<Graph>>(), false};
}

auto Solver::add_edge(const Node<Graph> &source, const Node<Graph> &target,
                      const std::shared_ptr<TransportEdge> &route)
    -> std::pair<Edge<Graph>, bool> {
  if (m_edge_by_name.contains(route->code)) {
    return {m_edge_by_name.at(route->code), true};
  }
  route->update(m_graph[source], m_graph[target]);
  auto [edge, inserted] = boost::add_edge(source, target, route, m_graph);
  if (inserted) {
    m_edge_by_name[route->code] = edge;
  }
  return {edge, inserted};
}

template <>
auto Solver::find_path<PathTraversalMode::FORWARD, VehicleType::AIR>(
    const Node<Graph> &source, const Node<Graph> &target, CLOCK start) const
    -> std::shared_ptr<Segment> {
  using FilterType = FilterByVehicleType<Graph, VehicleType::AIR>;
  using FilteredGraph = boost::filtered_graph<Graph, FilterType>;
  FilterType filter{&m_graph};
  FilteredGraph filtered_graph(m_graph, filter);
  return path_forward(source, target, start, filtered_graph);
}

template <>
auto Solver::find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
    const Node<Graph> &source, const Node<Graph> &target, CLOCK start) const
    -> std::shared_ptr<Segment> {
  using FilterType = FilterByVehicleType<Graph, VehicleType::SURFACE>;
  using FilteredGraph = boost::filtered_graph<Graph, FilterType>;
  FilterType filter{&m_graph};
  FilteredGraph filtered_graph(m_graph, filter);
  return path_forward(source, target, start, filtered_graph);
}

template <>
auto Solver::find_path<PathTraversalMode::REVERSE, VehicleType::AIR>(
    const Node<Graph> &source, const Node<Graph> &target, CLOCK start) const
    -> std::shared_ptr<Segment> {
  using REVERSED_GRAPH = boost::reverse_graph<Graph, const Graph &>;
  using FilterType = FilterByVehicleType<REVERSED_GRAPH, VehicleType::AIR>;
  using FilteredGraph = boost::filtered_graph<REVERSED_GRAPH, FilterType>;
  REVERSED_GRAPH reversed_graph = boost::make_reverse_graph(m_graph);
  FilterType filter{&reversed_graph};
  FilteredGraph filtered_graph(reversed_graph, filter);
  return path_reverse(source, target, start, filtered_graph);
}

template <>
auto Solver::find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
    const Node<Graph> &source, const Node<Graph> &target, CLOCK start) const
    -> std::shared_ptr<Segment> {
  using REVERSED_GRAPH = boost::reverse_graph<Graph, const Graph &>;
  using FilterType = FilterByVehicleType<REVERSED_GRAPH, VehicleType::SURFACE>;
  using FilteredGraph = boost::filtered_graph<REVERSED_GRAPH, FilterType>;
  REVERSED_GRAPH reversed_graph = boost::make_reverse_graph(m_graph);
  FilterType filter{&reversed_graph};
  FilteredGraph filtered_graph(reversed_graph, filter);
  return path_reverse(source, target, start, filtered_graph);
}

auto Solver::show() const -> std::string {
  return std::format("Graph<{}, {}>", boost::num_vertices(m_graph),
                     boost::num_edges(m_graph));
}

auto Solver::show_all() const -> std::string {
  std::vector<std::string> output;

  for (auto vertex : boost::make_iterator_range(boost::vertices(m_graph))) {
    auto node = m_graph[vertex];
    output.push_back(node->code);
  }

  for (auto edge : boost::make_iterator_range(boost::edges(m_graph))) {
    auto route = m_graph[edge];
    auto source = boost::source(edge, m_graph);
    auto target = boost::target(edge, m_graph);
    output.push_back(std::format("{}: {} TO {}", route->code,
                                 m_graph[source]->code, m_graph[target]->code));
  }

  return std::accumulate(
      output.begin(), output.end(), std::string{},
      [](const std::string &acc, const std::string &arg) -> std::string {
        return std::format("{}\n{}", acc, arg);
      });
}
