#include "solver.hxx"
#include "date_utils.hxx"                        // for CLOCK
#include "graph_helpers.hxx"                     // for FilterByVehicleType
#include "transportation.hxx"                    // for VehicleType, AIR
#include <boost/graph/detail/adjacency_list.hpp> // for get, num_vertices
#include <boost/graph/detail/edge.hpp>           // for operator!=, operator==
#include <boost/graph/filtered_graph.hpp>        // for filtered_graph
#include <boost/graph/reverse_graph.hpp>         // for get, make_reverse_g...
#include <boost/iterator/iterator_facade.hpp>    // for operator!=, operator++
#include <string>                                // for string

std::pair<Node<Graph>, bool>
Solver::add_node(std::string_view node_code_or_name) const
{
  if (vertex_by_name.contains(node_code_or_name))
    return { vertex_by_name.at(node_code_or_name), true };
  return { Graph::null_vertex(), false };
}

std::pair<Node<Graph>, bool>
Solver::add_node(const TransportCenter& center)
{
  auto created = add_node(center.code);

  if (created.second)
    return created;
  Node<Graph> node = boost::add_vertex(center, graph);
  vertex_by_name[center.name] = node;
  vertex_by_name[center.code] = node;
  return { node, true };
}

std::pair<Edge<Graph>, bool>
Solver::add_edge(std::string_view edge_code) const
{
  if (edge_by_name.contains(edge_code))
    return { edge_by_name.at(edge_code), true };
  return { null_edge<Edge<Graph>>(), false };
}

std::pair<Edge<Graph>, bool>
Solver::add_edge(const Node<Graph>& source,
                 const Node<Graph>& target,
                 const TransportEdge& route)
{
  if (edge_by_name.contains(route.code))
    return { edge_by_name.at(route.code), true };
  return boost::add_edge(source, target, route, graph);
}

template<>
void
Solver::operator()<PathTraversalMode::FORWARD, VehicleType::AIR>(
  const Node<Graph>& source,
  CLOCK start) const
{
  typedef FilterByVehicleType<Graph, VehicleType::AIR> FilterType;
  typedef boost::filtered_graph<Graph, FilterType> FilteredGraph;
  FilterType filter{ &graph };
  FilteredGraph filtered_graph(graph, filter);
  path_forward(source, start, filtered_graph);
}

template<>
void
Solver::operator()<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
  const Node<Graph>& source,
  CLOCK start) const
{
  typedef FilterByVehicleType<Graph, VehicleType::SURFACE> FilterType;
  typedef boost::filtered_graph<Graph, FilterType> FilteredGraph;
  FilterType filter{ &graph };
  FilteredGraph filtered_graph(graph, filter);
  path_forward(source, start, filtered_graph);
}

template<>
void
Solver::operator()<PathTraversalMode::REVERSE, VehicleType::AIR>(
  const Node<Graph>& source,
  CLOCK start) const
{
  typedef boost::reverse_graph<Graph, const Graph&> REVERSED_GRAPH;
  typedef FilterByVehicleType<REVERSED_GRAPH, VehicleType::AIR> FilterType;
  typedef boost::filtered_graph<REVERSED_GRAPH, FilterType> FilteredGraph;
  REVERSED_GRAPH reversed_graph = boost::make_reverse_graph(graph);
  FilterType filter{ &reversed_graph };
  FilteredGraph filtered_graph(reversed_graph, filter);
  path_reverse(source, start, filtered_graph);
}

template<>
void
Solver::operator()<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
  const Node<Graph>& source,
  CLOCK start) const
{
  typedef boost::reverse_graph<Graph, const Graph&> REVERSED_GRAPH;
  typedef FilterByVehicleType<REVERSED_GRAPH, VehicleType::SURFACE> FilterType;
  typedef boost::filtered_graph<REVERSED_GRAPH, FilterType> FilteredGraph;
  REVERSED_GRAPH reversed_graph = boost::make_reverse_graph(graph);
  FilterType filter{ &reversed_graph };
  FilteredGraph filtered_graph(reversed_graph, filter);
  path_reverse(source, start, filtered_graph);
}
