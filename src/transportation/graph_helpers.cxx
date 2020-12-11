#include "transportation/graph_helpers.hxx"
#include "transportation/network_ownership.hxx"
#include <bits/stdint-uintn.h>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/property_map/function_property_map.hpp>
#include <boost/property_map/property_map.hpp>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <date/date.h>

constexpr const uint16_t MINUTES_DIURNAL =
    std::chrono::minutes(std::chrono::hours(24)).count();

namespace moirai {
namespace transportation {
uint16_t TransportationEdge::time_departure(const uint16_t offset) const {
  return (time_arrival_source - offset) % MINUTES_DIURNAL;
}

uint16_t TransportationEdge::weight(
    const std::chrono::time_point<std::chrono::system_clock> start,
    const uint16_t offset) const {
  // Convert start to minutes
  // Get departure time
  // Calcualte wait time from departure - start_minutes MOD 24 * 60
  // Weight = wait_time + duration
  auto start_time = date::floor<std::chrono::minutes>(start);
  auto start_date = date::floor<date::days>(start);
  auto time_start = date::make_time(start - start_date);
  auto time_minutes = std::chrono::duration_cast<std::chrono::minutes>(
      time_start.to_duration());
  return (time_minutes.count() - time_departure(offset)) % MINUTES_DIURNAL;
}

detail::Edge BaseGraph::null_edge() {
  detail::Edge NULL_EDGE;
  memset((char *)&NULL_EDGE, 0xFF, sizeof(detail::Edge));
  return NULL_EDGE;
}

std::pair<detail::Vertex, bool>
BaseGraph::vertex(std::string_view identifier) const {
  if (vertices.contains(identifier))
    return {vertices.at(identifier), true};
  return {graph.null_vertex(), false};
}

std::pair<detail::Vertex, bool>
BaseGraph::add_vertex(const TransportationNode node) {
  if (vertices.contains(node.identifier)) {
    return {vertices.at(node.identifier), false};
  }
  detail::Vertex vertex = boost::add_vertex(node, graph);
  vertices[node.identifier] = vertex;
  return {vertex, true};
}

std::pair<detail::Edge, bool>
BaseGraph::edge(std::string_view identifier) const {
  if (edges.contains(identifier))
    return {edges.at(identifier), true};
  return {null_edge(), false};
}

std::pair<detail::Edge, bool>
BaseGraph::add_edge(const detail::Vertex source, const detail::Vertex target,
                    const TransportationEdge _edge) {
  if (edges.contains(_edge.identifier)) {
    return {edges.at(_edge.identifier), false};
  }
  auto created = boost::add_edge(source, target, _edge, graph);

  if (created.second)
    return created;
  return {null_edge(), true};
}

std::chrono::time_point<std::chrono::system_clock>
update_time(std::chrono::time_point<std::chrono::system_clock> start,
            uint16_t duration) {
  return start + std::chrono::minutes{duration};
}
} // namespace transportation
} // namespace moirai
