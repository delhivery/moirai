#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/graph_concepts.hpp>
#include <boost/graph/named_function_params.hpp>
#include <boost/graph/properties.hpp>
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

struct TransportationNode;
struct TransportationEdgeImpl;
struct FilterByMode;

typedef boost::adjacency_list<boost::vecS,
                              boost::vecS,
                              boost::directedS,
                              TransportationNode,
                              std::shared_ptr<TransportationEdgeImpl>>
  Graph;
typedef typename boost::graph_traits<Graph>::vertex_descriptor Vertex;
typedef typename boost::graph_traits<Graph>::edge_descriptor Edge;
typedef boost::filtered_graph<Graph, FilterByMode> FilteredGraph;

template<EdgeType edge_type, ProcessType process_type>
using Latency = std::chrono::seconds;

struct TransportationNode
{
  std::string code;
  std::string human_readable_name;

  Latency<EdgeType::CARTING, ProcessType::INBOUND> latency_carting_inbound;
  Latency<EdgeType::CARTING, ProcessType::OUTBOUND> latency_carting_outbound;

  Latency<EdgeType::LINEHAUL, ProcessType::INBOUND> latency_linehaul_inbound;
  Latency<EdgeType::LINEHAUL, ProcessType::OUTBOUND> latency_linehaul_outbound;

  template<EdgeType edge_type, ProcessType process_type>
  std::chrono::seconds latency() const;
};

template<>
std::chrono::seconds
TransportationNode::latency<EdgeType::CARTING, ProcessType::INBOUND>() const
{
  return latency_carting_inbound;
}

template<>
std::chrono::seconds
TransportationNode::latency<EdgeType::CARTING, ProcessType::OUTBOUND>() const
{
  return latency_carting_outbound;
}

template<>
std::chrono::seconds
TransportationNode::latency<EdgeType::LINEHAUL, ProcessType::INBOUND>() const
{
  return latency_linehaul_inbound;
}

template<>
std::chrono::seconds
TransportationNode::latency<EdgeType::LINEHAUL, ProcessType::OUTBOUND>() const
{
  return latency_linehaul_outbound;
}

class TransportationEdgeImpl
{
public:
  std::string code;
  date::hh_mm_ss<std::chrono::seconds> departure;
  std::chrono::seconds duration;

  template<TraversalMode T, class Graph>
  friend std::chrono::seconds weight(
    const Edge&,
    const FilteredGraph& graph,
    const std::chrono::time_point<std::chrono::system_clock>);

  virtual EdgeMode mode() const = 0;

  virtual EdgeType type() const = 0;
};

template<EdgeMode m, EdgeType e>
struct TransportationEdge : public TransportationEdgeImpl
{
public:
  EdgeMode mode() const { return m; }

  EdgeType type() const { return e; }
};

struct FilterByMode
{
  Graph* m_graph;
  EdgeMode m_mode;

  FilterByMode(Graph* m_graph, EdgeMode mode)
    : m_graph(m_graph)
    , m_mode(mode)
  {}

  bool operator()(Edge edge) const
  {
    std::shared_ptr<TransportationEdgeImpl> transport = (*m_graph)[edge];
    return transport->mode() == m_mode;
  }
};

template<TraversalMode>
std::chrono::seconds
weight(const Edge&,
       const FilteredGraph&,
       const std::chrono::time_point<std::chrono::system_clock>);

template<>
std::chrono::seconds
weight<TraversalMode::FORWARD>(
  const Edge& edge,
  const FilteredGraph& graph,
  const std::chrono::time_point<std::chrono::system_clock> start)
{
  TransportationNode source = graph[boost::source(edge, graph)];
  TransportationNode target = graph[boost::target(edge, graph)];
  std::shared_ptr<TransportationEdgeImpl> transport = graph[edge];

  EdgeType transport_type = transport->type();

  std::chrono::seconds offset_source, offset_target;

  switch (transport_type) {
    case EdgeType::CARTING:
      offset_source =
        source.latency<EdgeType::CARTING, ProcessType::OUTBOUND>();
      offset_target = target.latency<EdgeType::CARTING, ProcessType::INBOUND>();
      break;
    case EdgeType::LINEHAUL:
      offset_source =
        source.latency<EdgeType::LINEHAUL, ProcessType::OUTBOUND>();
      offset_target =
        target.latency<EdgeType::LINEHAUL, ProcessType::INBOUND>();
      break;
  }

  date::hh_mm_ss<std::chrono::seconds> offset_departure{
    transport->departure.to_duration() - offset_source
  };
  std::chrono::seconds offset_duration{ transport->duration + offset_target };

  date::hh_mm_ss<std::chrono::seconds> time_start{
    std::chrono::duration_cast<std::chrono::seconds>(
      start - std::chrono::floor<std::chrono::days>(start))
  };

  std::chrono::seconds wait_time =
    date::hh_mm_ss<std::chrono::seconds>(offset_departure.to_duration() -
                                         time_start.to_duration())
      .to_duration();
  return wait_time + offset_duration;
}

template<>
std::chrono::seconds
weight<TraversalMode::REVERSE>(
  const Edge& edge,
  const FilteredGraph& graph,
  const std::chrono::time_point<std::chrono::system_clock> start)
{
  // Edge is inbound edge to current vertex
  TransportationNode source = graph[boost::source(edge, graph)];
  TransportationNode target = graph[boost::target(edge, graph)];
  std::shared_ptr<TransportationEdgeImpl> transport = graph[edge];

  EdgeType transport_type = transport->type();

  std::chrono::seconds offset_source, offset_target;

  switch (transport_type) {
    case EdgeType::CARTING:
      offset_source =
        source.latency<EdgeType::CARTING, ProcessType::OUTBOUND>();
      offset_target = target.latency<EdgeType::CARTING, ProcessType::INBOUND>();
      break;
    case EdgeType::LINEHAUL:
      offset_source =
        source.latency<EdgeType::LINEHAUL, ProcessType::OUTBOUND>();
      offset_target =
        target.latency<EdgeType::LINEHAUL, ProcessType::INBOUND>();
      break;
  }

  date::hh_mm_ss<std::chrono::seconds> offset_arrival{
    transport->departure.to_duration() + transport->duration + offset_target
  };

  std::chrono::seconds offset_duration{ transport->duration + offset_source +
                                        offset_target };

  date::hh_mm_ss<std::chrono::seconds> time_start{
    std::chrono::duration_cast<std::chrono::seconds>(
      start - std::chrono::floor<std::chrono::days>(start))
  };

  std::chrono::seconds wait_time =
    date::hh_mm_ss<std::chrono::seconds>(time_start.to_duration() -
                                         offset_arrival.to_duration())
      .to_duration();
  return wait_time + offset_duration;
}

template<TraversalMode M>
struct Combinator
{
  std::chrono::time_point<std::chrono::system_clock> operator()(
    const std::chrono::time_point<std::chrono::system_clock>,
    const std::chrono::seconds) const;
};

template<>
std::chrono::time_point<std::chrono::system_clock>
Combinator<TraversalMode::REVERSE>::operator()(
  const std::chrono::time_point<std::chrono::system_clock> start,
  const std::chrono::seconds duration) const
{
  return start - duration;
}

template<>
std::chrono::time_point<std::chrono::system_clock>
Combinator<TraversalMode::FORWARD>::operator()(
  const std::chrono::time_point<std::chrono::system_clock> start,
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

  template<EdgeMode M, EdgeType T>
  std::pair<Edge, bool> add_edge(Vertex source,
                                 Vertex target,
                                 TransportationEdge<M, T> edge_props)
  {
    auto const has_edge = add_edge(edge_props.template code, graph);

    if (has_edge.second)
      return has_edge;

    std::shared_ptr<TransportationEdgeImpl> edge_ptr =
      std::make_shared(edge_props);
    auto const edge_created = boost::add_edge(source, target, edge_ptr, graph);

    if (edge_created.second)
      edge_by_name[edge_props.template code] = edge_created.first;
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
  FilterByMode filter(&graph, mode);
  FilteredGraph filtered_graph(graph, filter);

  std::vector<Vertex> predecessors(boost::num_vertices(filtered_graph));
  std::vector<std::chrono::system_clock::time_point> distances(
    boost::num_vertices(filtered_graph));

  auto weight_map =
    boost::make_transform_value_property_map<std::chrono::seconds>(
      [&filtered_graph, &distances, this](
        std::shared_ptr<TransportationEdgeImpl> edgeProps)
        -> std::chrono::seconds {
        Edge edge = edge_by_name[edgeProps->code];
        Vertex edge_source = boost::source(edge, filtered_graph);
        return weight<TraversalMode::FORWARD>(
          edge, filtered_graph, distances[edge_source]);
      },
      boost::get(boost::edge_bundle, filtered_graph));

  auto weight_by_edge =
    boost::make_function_property_map<Edge, std::chrono::seconds>(
      [&filtered_graph, &distances, this](Edge edge) -> std::chrono::seconds {
        Vertex edge_source = boost::source(edge, filtered_graph);
        return weight<TraversalMode::FORWARD>(
          edge, filtered_graph, distances[edge_source]);
      });

  Comparator<TraversalMode::FORWARD> compare;
  Combinator<TraversalMode::FORWARD> combine;

  std::chrono::system_clock::time_point zero =
    date::sys_days{ date::January / 1 / 1970 };

  std::chrono::system_clock::time_point inf =
    date::sys_days{ date::January / 1 / 2100 };

  auto weight = boost::get(weight_map, *boost::edges(filtered_graph).first);
  Edge firstEdge = *boost::edges(filtered_graph).first;
  bool matches = false;

  if (compare(combine(zero, boost::get(weight_map, firstEdge)), zero)) {
    matches = true;
  }

  auto predecessor_map = boost::make_iterator_property_map(
    predecessors.begin(), boost::get(boost::vertex_index, filtered_graph));

  auto distance_map = boost::make_iterator_property_map(
    distances.begin(), boost::get(boost::vertex_index, filtered_graph));

  /*  dijkstra_shortest_paths(
      filtered_graph,
      source,
      predecessor_map,
      distance_map,
      weight_map,
      boost::vertex_index_map(boost::get(boost::vertex_index, graph)),
      &compare,
      &combine,
      inf,
      zero);*/

  boost::dijkstra_shortest_paths(filtered_graph,
                                 source,
                                 boost::predecessor_map(&predecessors[0])
                                   .distance_map(&distances[0])
                                   .weight_map(weight_by_edge)
                                   .distance_compare(&compare)
                                   .distance_combine(&combine)
                                   .distance_zero(zero)
                                   .distance_inf(inf));
}

template<>
void
Solver::find_path<TraversalMode::REVERSE>(Vertex source,
                                          EdgeMode mode,
                                          std::chrono::seconds start)
{}
