#include "solver.hxx"
#include "graph_helpers.hxx"
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/reverse_graph.hpp>
#include <boost/property_map/transform_value_property_map.hpp>
#include <tuple>

Segment::Segment(std::shared_ptr<Center> from, std::shared_ptr<Route> via,
                 datetime distance)
    : mNode(from), mOutbound(via), mDistance(distance) {}

auto Segment::node() const -> std::shared_ptr<Center> { return mNode; }

auto Segment::distance() const -> datetime { return mDistance; }

////////////////////////////////////////////////////////////////////////////////////////
///    Solver begins here
////////////////////////////////////////////////////////////////////////////////////////

auto Solver::node(const VertexD<Graph> &nodeDescriptor) const
    -> std::shared_ptr<Center> {
  return mGraph[nodeDescriptor];
}

auto Solver::node(const std::shared_ptr<Center> center) -> VertexD<Graph> {

  if (not mNamedVertexMap.contains(center->code())) {
    mNamedVertexMap[center->code()] = boost::add_vertex(center, mGraph);
  }
  return mNamedVertexMap[center->code()];
}

auto Solver::edge(const EdgeD<Graph> &edgeDescriptor) const
    -> std::shared_ptr<Route> {
  return mGraph[edgeDescriptor];
}

auto Solver::edge(std::string_view source, std::string_view target,
                  const std::shared_ptr<Route> route) -> EdgeD<Graph> {
  if (not mNamedEdgeMap.contains(route->code())) {

    if (mNamedVertexMap.contains(source) and mNamedVertexMap.contains(target)) {
      auto sNode = mNamedVertexMap[source];
      auto tNode = mNamedVertexMap[target];
      auto [edgeD, added] = boost::add_edge(sNode, tNode, route, mGraph);

      if (added) {
        mNamedEdgeMap[route->code()] = edgeD;
      } else {
        return null_edge<EdgeD<Graph>>();
      }
    } else {
      return null_edge<EdgeD<Graph>>();
    }
  }
  return mNamedEdgeMap[route->code()];
}

/*
template <typename FilteredGraph>
auto Solver::solve(const Node<FilteredGraph> &src,
                   const Node<FilteredGraph> &tar, datetime start, datetime max,
                   const auto &wMap, const auto &comparator,
                   const FilteredGraph &fGraph) const -> std::vector<Segment> {
  using node_t = NodeD<FilteredGraph>;
  using edge_t = EdgeD<FilteredGraph>;
  using edge_pred_map_t = std::unordered_map<node_t, edge_t>;
  using edge_pred_pmap_t = associative_property_map<edge_pred_map_t>;

  edge_pred_map_t preds;
  edge_pred_pmap_t predsPmap(preds);
  std::vector<datetime> distances(num_vertices(fGraph), datetime::max());

  auto recorder = record_edge_predecessors(predsPmap, on_edge_relaxed());
  dijkstra_visitor<decltype(recorder)> visitor(recorder);

  dijkstra_shortest_paths(
      fGraph, src,
      distance_map(distances.data())
          .weight_map(wMap)
          .distance_compare(comparator)
          .distance_combine(
              [](datetime current, const WeightFunction &edgeCost) {
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
*/

template <typename FilteredGraph>
auto solve(const VertexD<FilteredGraph> &src, const VertexD<FilteredGraph> &tar,
           datetime start, datetime max, const auto &wMap,
           const auto &comparator, const FilteredGraph &fGraph)
    -> std::vector<Segment> {
  using node_t = VertexD<FilteredGraph>;
  using edge_t = EdgeD<FilteredGraph>;
  using edge_pred_map_t = std::unordered_map<node_t, edge_t>;
  using edge_pred_pmap_t = associative_property_map<edge_pred_map_t>;

  edge_pred_map_t preds;
  edge_pred_pmap_t predsPmap(preds);
  std::vector<datetime> distances(num_vertices(fGraph), datetime::max());

  auto recorder = record_edge_predecessors(predsPmap, on_edge_relaxed());
  dijkstra_visitor<decltype(recorder)> visitor(recorder);

  dijkstra_shortest_paths(
      fGraph, src,
      distance_map(distances.data())
          .weight_map(wMap)
          .distance_compare(comparator)
          .distance_combine(
              [](datetime current, const WeightFunction &edgeCost) {
                return edgeCost(current);
              })
          .distance_zero(start)
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

template <>
auto Solver::find_path<TraversalMode::FORWARD, Vehicle::AIR>(
    const VertexD<Graph> &source, const VertexD<Graph> &target,
    datetime start) const -> std::vector<Segment> {
  using FilterType = FilterByVehicle<Graph, Vehicle::AIR>;
  using FilteredGraph = boost::filtered_graph<Graph, FilterType>;

  FilterType filter{&mGraph};
  FilteredGraph fGraph(mGraph, filter);
  return solve(source, target, start, datetime::max(),
               make_transform_value_property_map(
                   [](const auto &edge) -> WeightFunction {
                     return edge->template weight<TraversalMode::FORWARD>();
                   },
                   boost::get(edge_bundle, fGraph)),
               std::less<>(), fGraph);
}

template <>
auto Solver::find_path<TraversalMode::FORWARD, Vehicle::SFC>(
    const VertexD<Graph> &source, const VertexD<Graph> &target,
    datetime start) const -> std::vector<Segment> {
  using FilterType = FilterByVehicle<Graph, Vehicle::SFC>;
  using FilteredGraph = boost::filtered_graph<Graph, FilterType>;

  FilterType filter{&mGraph};
  FilteredGraph fGraph(mGraph, filter);
  return solve(source, target, start, datetime::max(),
               boost::make_transform_value_property_map(
                   [](const auto &edge) -> WeightFunction {
                     return edge->template weight<TraversalMode::FORWARD>();
                   },
                   get(edge_bundle, fGraph)),
               std::less<>(), fGraph);
}

template <>
auto Solver::find_path<TraversalMode::REVERSE, Vehicle::AIR>(
    const VertexD<Graph> &source, const VertexD<Graph> &target,
    datetime start) const -> std::vector<Segment> {
  using ReversedGraph = boost::reverse_graph<Graph, const Graph &>;
  using FilterType = FilterByVehicle<ReversedGraph, Vehicle::AIR>;
  using FilteredGraph = boost::filtered_graph<ReversedGraph, FilterType>;

  ReversedGraph revGraph = boost::make_reverse_graph(mGraph);
  FilterType filter{&revGraph};
  FilteredGraph fGraph(revGraph, filter);
  return solve(source, target, start, datetime::min(),
               boost::make_transform_value_property_map(
                   [](const auto &edge) -> WeightFunction {
                     return edge->template weight<TraversalMode::REVERSE>();
                   },
                   get(edge_bundle, fGraph)),
               std::greater<>(), fGraph);
}

template <>
auto Solver::find_path<TraversalMode::REVERSE, Vehicle::SFC>(
    const VertexD<Graph> &source, const VertexD<Graph> &target,
    datetime start) const -> std::vector<Segment> {
  using ReversedGraph = boost::reverse_graph<Graph, const Graph &>;
  using FilterType = FilterByVehicle<ReversedGraph, Vehicle::SFC>;
  using FilteredGraph = boost::filtered_graph<ReversedGraph, FilterType>;

  ReversedGraph revGraph = boost::make_reverse_graph(mGraph);
  FilterType filter{&revGraph};
  FilteredGraph fGraph(revGraph, filter);
  return solve(source, target, start, datetime::min(),
               boost::make_transform_value_property_map(
                   [](const auto &edge) -> WeightFunction {
                     return edge->template weight<TraversalMode::REVERSE>();
                   },
                   get(edge_bundle, fGraph)),
               std::greater<>(), fGraph);
}
