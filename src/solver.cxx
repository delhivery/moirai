#include "solver.hxx"
#include "date_utils.hxx"
#include "graph_helpers.hxx"
#include "transportation.hxx"
#include <boost/graph/detail/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/property_map/function_property_map.hpp>
#include <boost/property_map/property_map.hpp>

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
  FilterByVehicleType<Graph, VehicleType::AIR> filter{ &graph };
  auto filtered_graph = boost::make_filtered_graph(graph, filter);

  std::vector<Node<Graph>> predecessors(boost::num_vertices(graph));
  auto predecessor_map = boost::make_iterator_property_map(
    predecessors.begin(), boost::get(boost::vertex_index, filtered_graph));

  std::vector<CLOCK> distances(boost::num_vertices(graph));
  auto distance_map = boost::make_iterator_property_map(
    distances.begin(), boost::get(boost::vertex_index, filtered_graph));

  Compare<PathTraversalMode::FORWARD> compare;

  CLOCK zero = CLOCK::min();
  CLOCK infi = CLOCK::max();

  auto weight_map =
    boost::get(&TransportEdge::weight<PathTraversalMode::FORWARD>, graph);

  boost::dijkstra_shortest_paths(
    filtered_graph,
    source,
    boost::predecessor_map(predecessor_map)
      .distance_map(distance_map)
      .weight_map(weight_map)
      .distance_compare(
        [&compare](CLOCK lhs, CLOCK rhs) { return compare(lhs, rhs); })
      .distance_compare([]<typename BinaryFunction>(
                          CLOCK start, BinaryFunction f) { return f(start); }));
}
