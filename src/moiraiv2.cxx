#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/graph_concepts.hpp>
#include <boost/graph/named_function_params.hpp>
#include <boost/graph/properties.hpp>
#include <boost/graph/reverse_graph.hpp>
#include <boost/property_map/function_property_map.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/property_map/transform_value_property_map.hpp>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <date/date.h>

enum EdgeMode
{
  SURFACE,
  AIR,
};

enum EdgeType
{
  CARTING,
  LINEHAUL,
};

enum ProcessType
{
  INBOUND,
  OUTBOUND,
};

enum TraversalMode
{
  FORWARD,
  REVERSE,
};

struct TransportationNode
{
  std::string code;
  std::string human_readable_name;

  std::chrono::seconds latency_carting_inbound;
  std::chrono::seconds latency_carting_outbound;

  std::chrono::seconds latency_linehaul_inbound;
  std::chrono::seconds latency_linehaul_outbound;

  std::chrono::seconds latency(EdgeType edge_type,
                               ProcessType process_type) const
  {
    if (edge_type == EdgeType::CARTING) {
      if (process_type == ProcessType::INBOUND)
        return latency_carting_inbound;
      return latency_carting_outbound;
    }

    if (process_type == ProcessType::INBOUND)
      return latency_linehaul_inbound;
    return latency_linehaul_outbound;
  }
};

struct TransportationEdge;

typedef boost::adjacency_list<boost::vecS,
                              boost::vecS,
                              boost::bidirectionalS,
                              TransportationNode,
                              TransportationEdge>
  Graph;
typedef boost::reverse_graph<Graph> ReversedGraph;

typedef typename boost::graph_traits<Graph>::vertex_descriptor Vertex;
typedef typename boost::graph_traits<Graph>::edge_descriptor Edge;

class TransportationEdge
{
public:
  std::string code;
  date::hh_mm_ss<std::chrono::seconds> departure;
  std::chrono::seconds duration;
  EdgeMode mode;
  EdgeType type;

  std::chrono::seconds weight(const TransportationNode source,
                              const TransportationNode target,
                              const std::chrono::system_clock::time_point start,
                              const TraversalMode mode) const
  {
    std::chrono::seconds offset_source =
      source.latency(type, ProcessType::OUTBOUND);
    std::chrono::seconds offset_target =
      target.latency(type, ProcessType::INBOUND);

    date::hh_mm_ss<std::chrono::seconds> offset_departure{
      departure.to_duration() - offset_source
    };
    std::chrono::seconds offset_duration{ duration + offset_target };
    date::hh_mm_ss<std::chrono::seconds> time_start{
      std::chrono::duration_cast<std::chrono::seconds>(
        start - std::chrono::floor<std::chrono::days>(start))
    };

    std::chrono::seconds wait_time =
      date::hh_mm_ss<std::chrono::seconds>(offset_departure.to_duration() +
                                           time_start.to_duration())
        .to_duration();
    return wait_time + offset_duration;
  }
};
template<class G>
struct FilterByMode
{
  G* graph;
  EdgeMode mode;

  FilterByMode() = default;

  FilterByMode(G* graph, EdgeMode mode)
    : graph(graph)
    , mode(mode)
  {}

  bool operator()(typename boost::graph_traits<G>::edge_descriptor edge) const
  {
    TransportationEdge transport = (*graph)[edge];
    return transport.mode == mode;
  }
};

template<TraversalMode M>
struct Combinator
{
  std::chrono::system_clock::time_point operator()(
    const std::chrono::system_clock::time_point,
    const std::chrono::seconds) const;
};

template<>
std::chrono::system_clock::time_point
Combinator<TraversalMode::REVERSE>::operator()(
  const std::chrono::system_clock::time_point start,
  const std::chrono::seconds duration) const
{
  return start - duration;
}

template<>
std::chrono::system_clock::time_point
Combinator<TraversalMode::FORWARD>::operator()(
  const std::chrono::system_clock::time_point start,
  const std::chrono::seconds duration) const
{
  return start + duration;
}

template<TraversalMode Mode>
struct Comparator
{
  bool operator()(const std::chrono::system_clock::time_point,
                  const std::chrono::system_clock::time_point) const;
};

template<>
bool
Comparator<TraversalMode::FORWARD>::operator()(
  const std::chrono::system_clock::time_point lhs,
  const std::chrono::system_clock::time_point rhs) const
{
  return lhs < rhs;
}

template<>
bool
Comparator<TraversalMode::REVERSE>::operator()(
  const std::chrono::system_clock::time_point lhs,
  const std::chrono::system_clock::time_point rhs) const
{
  return rhs < lhs;
}

class Solver
{
  Graph graph;
  std::map<std::string_view, Vertex> vertex_by_name;
  std::map<std::string_view, Edge> edge_by_name;

  std::pair<Vertex, bool> add_vertex(std::string_view node_name)
  {
    if (vertex_by_name.contains(node_name))
      return { vertex_by_name.at(node_name), true };
    return { Graph::null_vertex(), false };
  }

  std::pair<Vertex, bool> add_vertex(TransportationNode node)
  {
    auto const has_vertex = add_vertex(node.code);

    if (has_vertex.second) {
      return has_vertex;
    }

    Vertex vertex = boost::add_vertex(node, graph);
    vertex_by_name[node.code] = vertex;
    return { vertex, true };
  }

  static Edge null_edge()
  {
    Edge NULL_EDGE;
    memset((char*)&NULL_EDGE, 0xFF, sizeof(Edge));
    return NULL_EDGE;
  }

  std::pair<Edge, bool> add_edge(std::string_view edge_name)
  {
    if (edge_by_name.contains(edge_name))
      return { edge_by_name.at(edge_name), true };
    return { null_edge(), false };
  }

  std::pair<Edge, bool> add_edge(Vertex source,
                                 Vertex target,
                                 TransportationEdge edge_props)
  {
    auto const has_edge = add_edge(edge_props.code);

    if (has_edge.second)
      return has_edge;

    auto const edge_created =
      boost::add_edge(source, target, edge_props, graph);

    if (edge_created.second)
      edge_by_name[edge_props.code] = edge_created.first;
    return edge_created;
  }

  template<TraversalMode>
  void find_path(Vertex source, EdgeMode, std::chrono::seconds start);
};

template<>
void
Solver::find_path<TraversalMode::FORWARD>(Vertex source,
                                          EdgeMode mode,
                                          std::chrono::seconds start)
{
  FilterByMode filter{ &graph, mode };
  auto filtered_graph = boost::make_filtered_graph(graph, filter);

  std::vector<Vertex> predecessors(boost::num_vertices(filtered_graph));

  std::vector<std::chrono::system_clock::time_point> distances(
    boost::num_vertices(filtered_graph));

  auto weight_by_edge = boost::make_function_property_map<
    typename boost::graph_traits<Graph>::edge_descriptor,
    std::chrono::seconds>([&filtered_graph, &distances, this](
                            typename boost::graph_traits<Graph>::edge_descriptor
                              edge) -> std::chrono::seconds {
    Vertex source = boost::source(edge, filtered_graph);
    Vertex target = boost::target(edge, filtered_graph);
    TransportationEdge edge_details = filtered_graph[edge];

    return edge_details.weight(
      graph[source], graph[target], distances[source], TraversalMode::FORWARD);
  });

  Comparator<TraversalMode::FORWARD> compare;
  Combinator<TraversalMode::FORWARD> combine;

  std::chrono::system_clock::time_point zero =
    date::sys_days{ date::January / 1 / 1970 };

  std::chrono::system_clock::time_point inf =
    date::sys_days{ date::January / 1 / 2100 };

  auto predecessor_map = boost::make_iterator_property_map(
    predecessors.begin(), boost::get(boost::vertex_index, filtered_graph));

  auto distance_map = boost::make_iterator_property_map(
    distances.begin(), boost::get(boost::vertex_index, filtered_graph));

  boost::dijkstra_shortest_paths(
    filtered_graph,
    source,
    boost::predecessor_map(predecessor_map)
      .distance_map(distance_map)
      .weight_map(weight_by_edge)
      .distance_compare([&compare](std::chrono::system_clock::time_point lhs,
                                   std::chrono::system_clock::time_point rhs) {
        return compare(lhs, rhs);
      })
      .distance_combine([&combine](std::chrono::system_clock::time_point start,
                                   std::chrono::seconds duration) {
        return combine(start, duration);
      })
      .distance_zero(zero)
      .distance_inf(inf));
}

template<>
void
Solver::find_path<TraversalMode::REVERSE>(Vertex source,
                                          EdgeMode mode,
                                          std::chrono::seconds start)
{
  auto reversed_graph = boost::make_reverse_graph(graph);
  FilterByMode filter{ &reversed_graph, mode };
  auto filtered_graph = boost::make_filtered_graph(reversed_graph, filter);

  std::vector<Vertex> predecessors(boost::num_vertices(filtered_graph));
  std::vector<std::chrono::system_clock::time_point> distances(
    boost::num_vertices(filtered_graph));

  auto weight_by_edge = boost::make_function_property_map<
    typename boost::graph_traits<ReversedGraph>::edge_descriptor,
    std::chrono::seconds>(
    [&filtered_graph, &distances, this](
      typename boost::graph_traits<ReversedGraph>::edge_descriptor edge)
      -> std::chrono::seconds {
      Vertex source = boost::source(edge, filtered_graph);
      Vertex target = boost::target(edge, filtered_graph);
      TransportationEdge edge_details = filtered_graph[edge];

      return edge_details.weight(graph[source],
                                 graph[target],
                                 distances[source],
                                 TraversalMode::REVERSE);
    });

  Comparator<TraversalMode::REVERSE> compare;
  Combinator<TraversalMode::REVERSE> combine;

  std::chrono::system_clock::time_point zero =
    date::sys_days{ date::January / 1 / 2100 };

  std::chrono::system_clock::time_point inf =
    date::sys_days{ date::January / 1 / 1970 };

  auto predecessor_map = boost::make_iterator_property_map(
    predecessors.begin(), boost::get(boost::vertex_index, filtered_graph));

  auto distance_map = boost::make_iterator_property_map(
    distances.begin(), boost::get(boost::vertex_index, filtered_graph));

  boost::dijkstra_shortest_paths(
    filtered_graph,
    source,
    boost::predecessor_map(predecessor_map)
      .distance_map(distance_map)
      .weight_map(weight_by_edge)
      .distance_compare([&compare](std::chrono::system_clock::time_point lhs,
                                   std::chrono::system_clock::time_point rhs) {
        return compare(lhs, rhs);
      })
      .distance_combine([&combine](std::chrono::system_clock::time_point start,
                                   std::chrono::seconds duration) {
        return combine(start, duration);
      })
      .distance_zero(zero)
      .distance_inf(inf));
}
