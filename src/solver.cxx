#include "solver.hxx"
#include "date_utils.hxx"                        // for datetime
#include "graph_helpers.hxx"                     // for FilterByVehicleType
#include "transportation.hxx"                    // for VehicleType, AIR
#include <boost/graph/detail/adjacency_list.hpp> // for get, num_vertices
#include <boost/graph/detail/edge.hpp>           // for operator!=, operator==
#include <boost/graph/filtered_graph.hpp>        // for filtered_graph
#include <boost/graph/reverse_graph.hpp>         // for get, make_reverse_g...
#include <boost/iterator/iterator_facade.hpp>    // for operator!=, operator++
#include <fmt/format.h>
#include <numeric>
#include <string> // for string

auto
Solver::add_node(const std::string& nodeCodeOrName) const
  -> std::pair<Node<Graph>, bool>
{
  if (mNamedVertexMap.contains(nodeCodeOrName)) {
    return { mNamedVertexMap.at(nodeCodeOrName), true };
  }
  return { Graph::null_vertex(), false };
}

auto
Solver::add_node(const std::shared_ptr<TransportCenter>& center)
  -> std::pair<Node<Graph>, bool>
{
  auto created = add_node(center->code());

  if (created.second) {
    return created;
  }
  Node<Graph> node = boost::add_vertex(center, graph);
  mNamedVertexMap[center->code()] = node;
  return { node, true };
}

auto
Solver::get_node(const Node<Graph> node) const
  -> std::shared_ptr<TransportCenter>
{
  return graph[node];
}

auto
Solver::add_edge(const std::string& edgeCode) const
  -> std::pair<Edge<Graph>, bool>
{
  if (mNamedEdgeMap.contains(edgeCode)) {
    return { mNamedEdgeMap.at(edgeCode), true };
  }
  return { null_edge<Edge<Graph>>(), false };
}

auto
Solver::add_edge(const Node<Graph>& source,
                 const Node<Graph>& target,
                 const std::shared_ptr<TransportEdge>& edge)
  -> std::pair<Edge<Graph>, bool>
{
  if (mNamedEdgeMap.contains(edge->code())) {
    return { mNamedEdgeMap.at(edge->code()), true };
  }
  return boost::add_edge(source, target, edge, graph);
}

template<typename FilteredGraph>
auto
Solver::solve(const Node<FilteredGraph>& src,
              const Node<FilteredGraph>& tar,
              datetime beg,
              datetime max,
              const auto& wMap,
              const auto& comparator,
              const FilteredGraph& fGraph) const -> std::vector<Segment>
{
  using node_t = Node<FilteredGraph>;
  using edge_t = Edge<FilteredGraph>;
  using edge_pred_map_t = std::unordered_map<node_t, edge_t>;
  using edge_pred_pmap_t = associative_property_map<edge_pred_map_t>;

  edge_pred_map_t preds;
  edge_pred_pmap_t predsPmap(preds);
  std::vector<datetime> distances(num_vertices(fGraph), datetime::max());

  auto recorder = record_edge_predecessors(predsPmap, on_edge_relaxed());
  dijkstra_visitor<decltype(recorder)> visitor(recorder);

  dijkstra_shortest_paths(
    fGraph,
    src,
    distance_map(distances.data())
      .weight_map(wMap)
      .distance_compare(comparator)
      .distance_combine([](datetime current, const WeightFunction& edgeCost) {
        return edgeCost(current);
      })
      .distance_zero(beg)
      .distance_inf(max)
      .visitor(visitor));

  std::vector<Segment> path;

  node_t node = tar;
  datetime distance = distances[node];

  for (; node != src; node = source(predsPmap[node], fGraph)) {
    auto outEdge = predsPmap[node];
    distance = distances[node];

    if (distance == max) {
      break;
    }
    path.emplace_back(fGraph[node], fGraph[outEdge], distance);
  }

  if (node == src) {
    path.emplace_back(fGraph[node], nullptr, distances[node]);
  }
  std::reverse(path.begin(), path.end());
  return path;
}

template<>
auto
Solver::find_path<PathTraversalMode::FORWARD, VehicleType::AIR>(
  const Node<Graph>& source,
  const Node<Graph>& target,
  datetime start) const -> std::vector<Segment>
{
  using FilterType = FilterByVehicleType<Graph, VehicleType::AIR>;
  using FilteredGraph = boost::filtered_graph<Graph, FilterType>;

  FilterType filter{ &graph };
  FilteredGraph fGraph(graph, filter);
  return solve(source,
               target,
               start,
               datetime::max(),
               make_transform_value_property_map(
                 [](const auto& edge) -> WeightFunction {
                   return edge->template weight<PathTraversalMode::FORWARD>();
                 },
                 boost::get(edge_bundle, fGraph)),
               std::less<>(),
               fGraph);
}

template<>
auto
Solver::find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
  const Node<Graph>& source,
  const Node<Graph>& target,
  datetime start) const -> std::vector<Segment>
{
  using FilterType = FilterByVehicleType<Graph, VehicleType::SURFACE>;
  using FilteredGraph = boost::filtered_graph<Graph, FilterType>;

  FilterType filter{ &graph };
  FilteredGraph fGraph(graph, filter);
  return solve(source,
               target,
               start,
               datetime::max(),
               boost::make_transform_value_property_map(
                 [](const auto& edge) -> WeightFunction {
                   return edge->template weight<PathTraversalMode::FORWARD>();
                 },
                 get(edge_bundle, fGraph)),
               std::less<>(),
               fGraph);
}

template<>
auto
Solver::find_path<PathTraversalMode::REVERSE, VehicleType::AIR>(
  const Node<Graph>& source,
  const Node<Graph>& target,
  datetime start) const -> std::vector<Segment>
{
  using ReversedGraph = boost::reverse_graph<Graph, const Graph&>;
  using FilterType = FilterByVehicleType<ReversedGraph, VehicleType::AIR>;
  using FilteredGraph = boost::filtered_graph<ReversedGraph, FilterType>;

  ReversedGraph revGraph = boost::make_reverse_graph(graph);
  FilterType filter{ &revGraph };
  FilteredGraph fGraph(revGraph, filter);
  return solve(source,
               target,
               start,
               datetime::min(),
               boost::make_transform_value_property_map(
                 [](const auto& edge) -> WeightFunction {
                   return edge->template weight<PathTraversalMode::REVERSE>();
                 },
                 get(edge_bundle, fGraph)),
               std::greater<>(),
               fGraph);
}

template<>
auto
Solver::find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
  const Node<Graph>& source,
  const Node<Graph>& target,
  datetime start) const -> std::vector<Segment>
{
  using ReversedGraph = boost::reverse_graph<Graph, const Graph&>;
  using FilterType = FilterByVehicleType<ReversedGraph, VehicleType::SURFACE>;
  using FilteredGraph = boost::filtered_graph<ReversedGraph, FilterType>;

  ReversedGraph revGraph = boost::make_reverse_graph(graph);
  FilterType filter{ &revGraph };
  FilteredGraph fGraph(revGraph, filter);
  return solve(source,
               target,
               start,
               datetime::min(),
               boost::make_transform_value_property_map(
                 [](const auto& edge) -> WeightFunction {
                   return edge->template weight<PathTraversalMode::REVERSE>();
                 },
                 get(edge_bundle, fGraph)),
               std::greater<>(),
               fGraph);
}
