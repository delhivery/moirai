#ifndef MOIRAI_SOLVER_LATEST_FORWARD
#define MOIRAI_SOLVER_LATEST_FORWARD
#include "transportation/graph_helpers.hxx"
#include "transportation/network_ownership.hxx"
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/property_map/function_property_map.hpp>
#include <ranges>

extern const uint16_t MINUTES_DIURNAL;

namespace moirai {
namespace solver {
namespace detail {
struct Combine
{
public:
  std::chrono::time_point<std::chrono::system_clock> operator()(
    const std::chrono::time_point<std::chrono::system_clock>,
    const int16_t) const;
};

template<transportation::Mode mode>
struct Predicate
{
  bool operator()(transportation::detail::Edge edge) const
  {
    return mode == (*m_graph)[edge].mode;
  }

  bool operator()(transportation::detail::Vertex vertex) const { return true; }

  transportation::detail::Graph* m_graph;
};
} // namespace detail

template<transportation::Mode mode>
class LatestForward : public transportation::BaseGraph
{
private:
  using Graph = transportation::detail::Graph;
  using Vertex = transportation::detail::Vertex;
  using Edge = transportation::detail::Edge;
  using Center = transportation::TransportationNode;
  using Connection = transportation::TransportationEdge;
  using FilteredGraph = boost::filtered_graph<Graph, detail::Predicate<mode>>;

  FilteredGraph m_graph;

public:
  LatestForward()
  {
    detail::Predicate<mode> predicate{ &graph };
    m_graph = FilteredGraph(graph, predicate);
  }

  void find_latest_path(
    std::string_view source,
    std::string_view target,
    const std::chrono::time_point<std::chrono::system_clock> start,
    const std::chrono::time_point<std::chrono::system_clock> end) const
  {
    auto lookup_source = vertex(source), lookup_target = vertex(target);

    if (lookup_source.second and lookup_target.second) {
      Vertex source = lookup_source.first, target = lookup_target.first;
      auto candidate_edges = boost::out_edges(source, m_graph);

      uint16_t maximal_hold_delay =
        (std::chrono::duration_cast<std::chrono::minutes>(end - start))
          .count() /
        MINUTES_DIURNAL;

      std::ranges::for_each(
        std::ranges::reverse_view{
          std::ranges::iota_view{ 0, maximal_hold_delay } },
        [this, &start](uint8_t days_hold_delay) {
          std::chrono::time_point<std::chrono::system_clock> delayed_start =
            start + std::chrono::minutes{ days_hold_delay * MINUTES_DIURNAL };
          std::for_each(
            candidate_edges, [this, &delayed_start](Edge candidate_edge) {
              Connection candidate_connection = m_graph[candidate_edge];
              Vertex candidate_source = boost::target(candidate_edge, graph);

              uint16_t delayed_offset = 0;
              uint32_t delayed_source_start =
                delayed_start +
                candidate_connection.weight(delayed_start, delayed_offset);
            });
        });

      for (int delay : std::ranges::iota_view{ 1, 10 }) {
        std::for_each(auto const candidate_edge : candidate_edges)
        {
          const uint32_t updated_start = start + delay * MINUTES_DIURNAL;
        }
      }
    }
  }
};
} // namespace solver
} // namespace moirai
#endif
