#ifndef MOIRAI_SOLVER_EARLIEST_FORWARD
#define MOIRAI_SOLVER_EARLIEST_FORWARD
#include "transportation/graph_helpers.hxx"
#include "transportation/network_ownership.hxx"
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/property_map/function_property_map.hpp>

namespace moirai {
namespace solver {
namespace detail {
struct Combine {
public:
  std::chrono::time_point<std::chrono::system_clock>
  operator()(const std::chrono::time_point<std::chrono::system_clock>,
             const int16_t) const;
};

template <transportation::Mode mode> struct Predicate {
  bool operator()(transportation::detail::Edge edge) const {
    return mode == (*m_graph)[edge].mode;
  }

  bool operator()(transportation::detail::Vertex vertex) const { return true; }

  transportation::detail::Graph *m_graph;
};
} // namespace detail

template <transportation::Mode mode>
class EarliestForward : public transportation::BaseGraph {
private:
  using Graph = transportation::detail::Graph;
  using Vertex = transportation::detail::Vertex;
  using Edge = transportation::detail::Edge;
  using Center = transportation::TransportationNode;
  using Connection = transportation::TransportationEdge;
  using FilteredGraph = boost::filtered_graph<Graph, detail::Predicate<mode>>;

  FilteredGraph m_graph;

public:
  EarliestForward() {
    detail::Predicate<mode> predicate{&graph};
    m_graph = FilteredGraph(graph, predicate);
  }

  void find_path(std::string_view source, std::string_view target,
                 const uint32_t start) const {
    auto lookup_source = vertex(source), lookup_target = vertex(target);

    if (lookup_source.second and lookup_target.second) {
      Vertex source = lookup_source.first, target = lookup_target.first;

      std::vector<Vertex> predecessors(boost::num_vertices(m_graph));

      std::vector<std::chrono::time_point<std::chrono::system_clock>> distances(
          boost::num_vertices(m_graph));

      auto weight_map = boost::make_function_property_map<Edge>([this,
                                                                 &distances](
                                                                    Edge edge) {
        Vertex source = boost::source(edge, m_graph),
               target = boost::target(edge, m_graph);
        Center transportation_source = m_graph[source],
               transportation_target = m_graph[target];

        auto distance_source = distances.at(source);
        Connection transportation_edge = m_graph[edge];

        uint16_t offset_source = 0, offset_target = 0;

        if (transportation_edge.mode == transportation::Mode::CARTING) {
          offset_source = transportation_source.time_latency_outbound_carting;
          offset_target = transportation_target.time_latency_inbound_carting;
        } else {
          offset_source = transportation_source.time_latency_outbound_linehaul;
          offset_target = transportation_target.time_latency_inbound_linehaul;
        }
        return transportation_edge.weight(distance_source, offset_source);
      });
      detail::Combine combinator;

      boost::dijkstra_shortest_paths(
          m_graph, source,
          boost::predecessor_map(boost::make_iterator_property_map(
                                     predecessors.begin(),
                                     boost::get(boost::vertex_index, m_graph)))
              .distance_map(boost::make_iterator_property_map(
                  distances.begin(), boost::get(boost::vertex_index, m_graph)))
              .weight_map(weight_map)
              .distance_combine(&combinator));
    }
  }
};
} // namespace solver
} // namespace moirai
#endif
