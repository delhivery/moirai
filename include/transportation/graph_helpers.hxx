#ifndef MOIRAI_TRANSPORTATION_GRAPH_HELPERS
#define MOIRAI_TRANSPORTATION_GRAPH_HELPERS

#include "transportation/network_ownership.hxx"

#include <chrono>
#include <cstdint>
#include <string>

#include <boost/graph/adjacency_list.hpp>

namespace moirai {

namespace transportation {

struct TransportationNode;

struct TransportationEdge;

namespace detail {
typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS,
                              TransportationNode, TransportationEdge>
    Graph;
typedef boost::graph_traits<Graph>::vertex_descriptor Vertex;
typedef boost::graph_traits<Graph>::edge_descriptor Edge;

} // namespace detail

struct TransportationNode {
  std::string identifier;
  std::string human_readable_name;

  uint16_t time_latency_inbound_carting;
  uint16_t time_latency_outbound_carting;

  uint16_t time_latency_inbound_linehaul;
  uint16_t time_latency_outbound_linehaul;
};

struct TransportationEdge {
  std::string identifier;

  std::string human_readable_name;

  uint16_t time_arrival_source;
  uint16_t time_duration_travel;

  bool is_virtual;

  Mode mode;

public:
  uint16_t time_departure(const uint16_t) const;

  uint16_t weight(const std::chrono::time_point<std::chrono::system_clock>,
                  const uint16_t) const;
};

class BaseGraph {
private:
  std::map<std::string_view, detail::Vertex> vertices;
  std::map<std::string_view, detail::Edge> edges;

protected:
  detail::Graph graph;

public:
  BaseGraph() = default;

  static detail::Edge null_edge();

  std::pair<detail::Vertex, bool> vertex(std::string_view) const;

  std::pair<detail::Vertex, bool> add_vertex(const TransportationNode);

  std::pair<detail::Edge, bool> edge(std::string_view) const;

  std::pair<detail::Edge, bool> add_edge(const detail::Vertex,
                                         const detail::Vertex,
                                         const TransportationEdge);

  virtual void find_path(std::string_view, std::string_view, const uint32_t) const = 0;

  std::string to_string() const;
};
} // namespace transportation
} // namespace moirai
#endif
