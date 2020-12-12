#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <chrono>
#include <cmath>
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

typedef boost::adjacency_list<boost::vecS,
                              boost::vecS,
                              boost::directedS,
                              TransportationNode,
                              std::shared_ptr<TransportationEdgeImpl>>
  Graph;
typedef typename boost::graph_traits<Graph>::vertex_descriptor Vertex;
typedef typename boost::graph_traits<Graph>::edge_descriptor Edge;

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
  std::string code;
  date::hh_mm_ss<std::chrono::seconds> departure;
  std::chrono::seconds duration;

  template<TraversalMode traversal>
  friend std::chrono::seconds weight(
    const Edge&,
    const Graph&,
    const std::chrono::time_point<std::chrono::system_clock,
                                  std::chrono::seconds>);

  virtual EdgeMode mode() const = 0;

  virtual EdgeType type() const = 0;
};

template<EdgeMode m, EdgeType e>
struct TransportationEdge : public TransportationEdgeImpl
{
  EdgeMode mode() const { return m; }

  EdgeType type() const { return e; }
};

template<>
std::chrono::seconds
weight<TraversalMode::FORWARD>(
  const Edge& edge,
  const Graph& graph,
  const std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
    start)
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
    start - std::chrono::floor<std::chrono::days>(start)
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
  const Graph& graph,
  const std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
    start)
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
    start - std::chrono::floor<std::chrono::days>(start)
  };

  std::chrono::seconds wait_time =
    date::hh_mm_ss<std::chrono::seconds>(time_start.to_duration() -
                                         offset_arrival.to_duration())
      .to_duration();
  return wait_time + offset_duration;
}

template<TraversalMode Mode>
std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
combine(const std::chrono::time_point<std::chrono::system_clock,
                                      std::chrono::seconds>,
        const std::chrono::seconds);

template<>
std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
combine<TraversalMode::FORWARD>(
  const std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
    start,
  const std::chrono::seconds duration)
{
  return start + duration;
}

template<>
std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
combine<TraversalMode::REVERSE>(
  const std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
    start,
  const std::chrono::seconds duration)
{
  return start - duration;
}

template<TraversalMode Mode>
struct Comparator
{
  bool operator()(const std::chrono::system_clock::time_point,
                  const std::chrono::system_clock::time_point);
};

template<>
bool
Comparator<TraversalMode::FORWARD>::operator()(
  const std::chrono::system_clock::time_point lhs,
  const std::chrono::system_clock::time_point rhs)
{
  return lhs < rhs;
}

template<>
bool
Comparator<TraversalMode::REVERSE>::operator()(
  const std::chrono::system_clock::time_point lhs,
  const std::chrono::system_clock::time_point rhs)
{
  return rhs < lhs;
}
