#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <chrono>
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

struct TransportationNode;
struct TransportationEdgeImpl;

typedef boost::adjacency_list<boost::vecS,
                              boost::vecS,
                              boost::directedS,
                              TransportationNode,
                              std::unique_ptr<TransportationEdgeImpl>>
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
  std::chrono::seconds latency();
};

template<>
std::chrono::seconds
TransportationNode::latency<EdgeType::CARTING, ProcessType::INBOUND>()
{
  return latency_carting_inbound;
}

template<>
std::chrono::seconds
TransportationNode::latency<EdgeType::CARTING, ProcessType::OUTBOUND>()
{
  return latency_carting_outbound;
}

template<>
std::chrono::seconds
TransportationNode::latency<EdgeType::LINEHAUL, ProcessType::INBOUND>()
{
  return latency_linehaul_inbound;
}

template<>
std::chrono::seconds
TransportationNode::latency<EdgeType::LINEHAUL, ProcessType::OUTBOUND>()
{
  return latency_linehaul_outbound;
}

class TransportationEdgeImpl
{
  std::string code;
  date::hh_mm_ss<std::chrono::seconds> departure;
  std::chrono::seconds duration;

  friend std::chrono::seconds weight(
    const Edge& edge,
    const Graph& graph,
    const date::hh_mm_ss<std::chrono::seconds>);

  virtual EdgeMode mode() = 0;

  virtual EdgeType type() = 0;
};

template<EdgeMode m, EdgeType e>
struct TransportationEdge : public TransportationEdgeImpl
{
  EdgeMode mmode() { return m; }

  EdgeType type() { return e; }
};

std::chrono::seconds
weight(const Edge& edge,
       const Graph& graph,
       const date::hh_mm_ss<std::chrono::seconds> start)
{
  Vertex source = boost::source(edge, graph);
  Vertex target = boost::target(edge, graph);
}
