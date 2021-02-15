#include "solver.hxx"
#include "date_utils.hxx" // for CLOCK
#include "format.hxx"
#include "graph_helpers.hxx"                     // for FilterByVehicleType
#include "transportation.hxx"                    // for VehicleType, AIR
#include <boost/graph/detail/adjacency_list.hpp> // for get, num_vertices
#include <boost/graph/detail/edge.hpp>           // for operator!=, operator==
#include <boost/graph/filtered_graph.hpp>        // for filtered_graph
#include <boost/graph/reverse_graph.hpp>         // for get, make_reverse_g...
#include <boost/iterator/iterator_facade.hpp>    // for operator!=, operator++
#include <numeric>
#include <string> // for string

std::pair<Node<Graph>, bool>
Solver::add_node(std::string node_code_or_name) const
{
  if (vertex_by_name.contains(node_code_or_name))
    return { vertex_by_name.at(node_code_or_name), true };
  return { Graph::null_vertex(), false };
}

std::pair<Node<Graph>, bool>
Solver::add_node(std::shared_ptr<TransportCenter> center)
{
  auto created = add_node(center->code);

  if (created.second)
    return created;
  Node<Graph> node = boost::add_vertex(center, graph);
  vertex_by_name[center->code] = node;
  return { node, true };
}

std::shared_ptr<TransportCenter>
Solver::get_node(const Node<Graph> node) const
{
  return graph[node];
}

std::pair<Edge<Graph>, bool>
Solver::add_edge(std::string edge_code) const
{
  if (edge_by_name.contains(edge_code))
    return { edge_by_name.at(edge_code), true };
  return { null_edge<Edge<Graph>>(), false };
}

std::pair<Edge<Graph>, bool>
Solver::add_edge(const Node<Graph>& source,
                 const Node<Graph>& target,
                 std::shared_ptr<TransportEdge> route)
{
  if (edge_by_name.contains(route->code))
    return { edge_by_name.at(route->code), true };
  route->update(graph[source], graph[target]);
  return boost::add_edge(source, target, route, graph);
}

template<>
Path
Solver::find_path<PathTraversalMode::FORWARD, VehicleType::AIR>(
  const Node<Graph>& source,
  const Node<Graph>& target,
  CLOCK start) const
{
  typedef FilterByVehicleType<Graph, VehicleType::AIR> FilterType;
  typedef boost::filtered_graph<Graph, FilterType> FilteredGraph;
  FilterType filter{ &graph };
  FilteredGraph filtered_graph(graph, filter);
  return path_forward(source, target, start, filtered_graph);
}

template<>
Path
Solver::find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
  const Node<Graph>& source,
  const Node<Graph>& target,
  CLOCK start) const
{
  typedef FilterByVehicleType<Graph, VehicleType::SURFACE> FilterType;
  typedef boost::filtered_graph<Graph, FilterType> FilteredGraph;
  FilterType filter{ &graph };
  FilteredGraph filtered_graph(graph, filter);
  return path_forward(source, target, start, filtered_graph);
}

template<>
Path
Solver::find_path<PathTraversalMode::REVERSE, VehicleType::AIR>(
  const Node<Graph>& source,
  const Node<Graph>& target,
  CLOCK start) const
{
  typedef boost::reverse_graph<Graph, const Graph&> REVERSED_GRAPH;
  typedef FilterByVehicleType<REVERSED_GRAPH, VehicleType::AIR> FilterType;
  typedef boost::filtered_graph<REVERSED_GRAPH, FilterType> FilteredGraph;
  REVERSED_GRAPH reversed_graph = boost::make_reverse_graph(graph);
  FilterType filter{ &reversed_graph };
  FilteredGraph filtered_graph(reversed_graph, filter);
  return path_reverse(source, target, start, filtered_graph);
}

template<>
Path
Solver::find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
  const Node<Graph>& source,
  const Node<Graph>& target,
  CLOCK start) const
{
  typedef boost::reverse_graph<Graph, const Graph&> REVERSED_GRAPH;
  typedef FilterByVehicleType<REVERSED_GRAPH, VehicleType::SURFACE> FilterType;
  typedef boost::filtered_graph<REVERSED_GRAPH, FilterType> FilteredGraph;
  REVERSED_GRAPH reversed_graph = boost::make_reverse_graph(graph);
  FilterType filter{ &reversed_graph };
  FilteredGraph filtered_graph(reversed_graph, filter);
  return path_reverse(source, target, start, filtered_graph);
}

std::string
Solver::show() const
{
  return std::format(
    "Graph<{}, {}>", boost::num_vertices(graph), boost::num_edges(graph));
}

std::string
Solver::show_all() const
{
  std::vector<std::string> output;

  for (auto vertex : boost::make_iterator_range(boost::vertices(graph))) {
    auto node = graph[vertex];
    output.push_back(node->code);
  }

  for (auto edge : boost::make_iterator_range(boost::edges(graph))) {
    auto route = graph[edge];
    auto source = boost::source(edge, graph);
    auto target = boost::target(edge, graph);
    output.push_back(std::format(
      "{}: {} TO {}", route->code, graph[source]->code, graph[target]->code));
  }

  return std::accumulate(output.begin(),
                         output.end(),
                         std::string{},
                         [](const std::string& acc, const std::string& arg) {
                           return std::format("{}\n{}", acc, arg);
                         });
}
