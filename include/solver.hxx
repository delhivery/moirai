#pragma once

#include "date_utils.hxx"                                      // for CLOCK
#include "transportation.hxx"                                  // for Trans...
#include <boost/graph/adjacency_list.hpp>                      // for source
#include <boost/graph/dijkstra_shortest_paths.hpp>             // for dijks...
#include <boost/graph/filtered_graph.hpp>                      // for target
#include <boost/graph/graph_selectors.hpp>                     // for bidir...
#include <boost/graph/graph_traits.hpp>                        // for graph...
#include <boost/graph/named_function_params.hpp>               // for prede...
#include <boost/graph/properties.hpp>                          // for verte...
#include <boost/graph/visitors.hpp>                            // for visitor
#include <boost/pending/property.hpp>                          // for edge_...
#include <boost/property_map/property_map.hpp>                 // for make_...
#include <boost/property_map/transform_value_property_map.hpp> // for make_...
#include <chrono>                                              // for opera...
#include <iostream>                                            // for cout
#include <map>                                                 // for map
#include <string>                                              // for strin...
#include <utility>                                             // for pair
#include <vector>                                              // for vector

using Graph =
    boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS,
                          std::shared_ptr<TransportCenter>,
                          std::shared_ptr<TransportEdge>>;

template <class G> using Node = boost::graph_traits<G>::vertex_descriptor;

template <class G> using Edge = boost::graph_traits<G>::edge_descriptor;

// typedef std::vector<std::tuple<std::shared_ptr<TransportCenter>,
//                               std::shared_ptr<TransportCenter>,
//                               std::shared_ptr<TransportEdge>,
//                               CLOCK>>
//  Path;

struct Segment {
  std::shared_ptr<TransportCenter> node = nullptr;
  std::shared_ptr<TransportEdge> outbound = nullptr;
  std::shared_ptr<Segment> prev = nullptr;
  std::shared_ptr<Segment> next = nullptr;
  CLOCK distance;
};

class Solver {

private:
  Graph m_graph;
  std::map<std::string, Node<Graph>> m_vertex_by_name;
  std::map<std::string, Edge<Graph>> m_edge_by_name;

public:
  [[nodiscard]] auto add_node(const std::string &node_code_or_name) const
      -> std::pair<Node<Graph>, bool>;

  auto add_node(const std::shared_ptr<TransportCenter> &center)
      -> std::pair<Node<Graph>, bool>;

  [[nodiscard]] auto get_node(Node<Graph> node) const
      -> std::shared_ptr<TransportCenter>;

  [[nodiscard]] auto add_edge(const std::string &edge_code) const
      -> std::pair<Edge<Graph>, bool>;

  auto add_edge(const Node<Graph> &source, const Node<Graph> &target,
                const std::shared_ptr<TransportEdge> &route)
      -> std::pair<Edge<Graph>, bool>;

  [[nodiscard]] auto show() const -> std::string;

  [[nodiscard]] auto show_all() const -> std::string;

  template <typename FilteredGraph>
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  [[nodiscard]] auto path_forward(const Node<FilteredGraph> &source,
                                  const Node<FilteredGraph> &target,
                                  CLOCK start,
                                  const FilteredGraph &filtered_graph) const
      -> std::shared_ptr<Segment> {
    using predecessor_edge_map_t =
        std::map<Node<FilteredGraph>, Edge<FilteredGraph>>;
    using predecessor_edge_property_map_t =
        boost::associative_property_map<predecessor_edge_map_t>;

    predecessor_edge_map_t predecessors;
    predecessor_edge_property_map_t predecessors_property_map(predecessors);

    auto recorder = boost::record_edge_predecessors(predecessors_property_map,
                                                    boost::on_edge_relaxed());
    boost::dijkstra_visitor<decltype(recorder)> visitor(recorder);

    std::vector<CLOCK> distances(boost::num_vertices(filtered_graph));

    auto w_map = boost::make_transform_value_property_map(
        [](const std::shared_ptr<TransportEdge> &edge) -> auto {
          return edge->weight<PathTraversalMode::FORWARD>();
        },
        get(boost::edge_bundle, filtered_graph));

    boost::dijkstra_shortest_paths(
        filtered_graph, source,
        boost::distance_map(distances.data())
            .weight_map(w_map)
            .distance_compare(
                [](CLOCK lhs, CLOCK rhs) -> auto { return lhs < rhs; })
            .distance_combine([](CLOCK initial, COST cost) -> auto {
              CalcualateTraversalCost calculator;
              CLOCK computed =
                  calculator.template operator()<PathTraversalMode::FORWARD>(
                      initial, cost);
              if (computed < initial) {
                std::cout
                    << std::format(
                           "Found a lower cost {} from initial {}. Cost: {},{}",
                           format_clock(computed), format_clock(initial),
                           cost.first.count(), cost.second.count())
                    << '\n';
              }
              return computed;
            })
            .distance_zero(start)
            .distance_inf(CLOCK::max())
            .visitor(visitor));

    auto segment = std::make_shared<Segment>();
    segment->next = nullptr;
    segment->outbound = nullptr;

    for (Node<FilteredGraph> current = target; current != source;
         current = boost::source(predecessors_property_map[current],
                                 filtered_graph)) {

      auto edge_descriptor = predecessors_property_map[current];
      auto sour_descriptor = boost::source(edge_descriptor, filtered_graph);
      CLOCK distance = distances[current];

      if (distance == CLOCK::max()) {
        return nullptr;
      }

      // We are at current center
      // Set node
      segment->node = filtered_graph[current];
      // Set current center distance
      segment->distance = distances[current];

      // Create predecessor segment
      segment->prev = std::make_shared<Segment>();
      // Set predecessor's successor as self
      segment->prev->next = segment;

      // Set outbound edge on predecessor
      segment->prev->outbound = filtered_graph[edge_descriptor];

      // Move self to predecessor
      segment = segment->prev;

      /*path.push_back(std::make_tuple(filtered_graph[current],
                                     filtered_graph[sour_descriptor],
                                     filtered_graph[edge_descriptor],
                                     distance));*/
    }
    segment->node = filtered_graph[source];
    segment->distance = distances[source];
    segment->prev = nullptr;

    // We are at path origin
    return segment;
  }

  template <typename FilteredGraph>
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  [[nodiscard]] auto path_reverse(const Node<FilteredGraph> &source,
                                  const Node<FilteredGraph> &target,
                                  CLOCK start,
                                  const FilteredGraph &filtered_graph) const
      -> std::shared_ptr<Segment> {
    using predecessor_edge_map_t =
        std::map<Node<FilteredGraph>, Edge<FilteredGraph>>;
    using predecessor_edge_property_map_t =
        boost::associative_property_map<predecessor_edge_map_t>;

    predecessor_edge_map_t predecessors;
    predecessor_edge_property_map_t predecessors_property_map(predecessors);

    auto recorder = boost::record_edge_predecessors(predecessors_property_map,
                                                    boost::on_edge_relaxed());
    boost::dijkstra_visitor<decltype(recorder)> visitor(recorder);

    std::vector<CLOCK> distances(boost::num_vertices(filtered_graph));

    auto w_map = boost::make_transform_value_property_map(
        [](const std::shared_ptr<TransportEdge> &edge) -> auto {
          return edge->weight<PathTraversalMode::REVERSE>();
        },
        get(boost::edge_bundle, filtered_graph));

    boost::dijkstra_shortest_paths(
        filtered_graph, source,
        boost::distance_map(distances.data())
            .weight_map(w_map)
            .distance_compare(
                [](CLOCK lhs, CLOCK rhs) -> auto { return lhs > rhs; })
            .distance_combine([](CLOCK start, COST cost) -> auto {
              CalcualateTraversalCost calculator;
              return calculator.template operator()<PathTraversalMode::REVERSE>(
                  start, cost);
            })
            .distance_zero(start)
            .distance_inf(CLOCK::min())
            .visitor(visitor));

    auto segment = std::make_shared<Segment>();
    segment->prev = nullptr;

    for (Node<FilteredGraph> current = target; current != source;
         current = boost::source(predecessors_property_map[current],
                                 filtered_graph)) {
      auto edge_descriptor = predecessors_property_map[current];
      auto sour_descriptor = boost::source(edge_descriptor, filtered_graph);
      CLOCK distance = distances[current];

      if (distance == CLOCK::min()) {
        return nullptr;
      }

      // We are at current center
      // Set node as current center
      segment->node = filtered_graph[current];
      // Set current center distance
      segment->distance = distances[current];

      // Set successor edge node
      segment->outbound = filtered_graph[edge_descriptor];

      if (segment->outbound->movement == MovementType::CARTING) {
        segment->distance +=
            segment->node
                ->get_latency<MovementType::CARTING, ProcessType::OUTBOUND>();
      } else {
        segment->distance +=
            segment->node
                ->get_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>();
      }
      // Create a successor center
      segment->next = std::make_shared<Segment>();
      // Set self as successor's predecessor
      segment->next->prev = segment;

      // Move self to successor edge
      segment = segment->next;

      /*path.push_back(std::make_tuple(filtered_graph[current],
                                     filtered_graph[sour_descriptor],
                                     filtered_graph[edge_descriptor],
                                     distance));*/
    }
    segment->node = filtered_graph[source];
    segment->distance = distances[source];
    segment->next = nullptr;

    while (segment->prev != nullptr) {
      segment = segment->prev;
    }
    // We are at path origin center
    return segment;
  }

  template <PathTraversalMode P, VehicleType V = VehicleType::AIR>
  [[nodiscard]] auto find_path(const Node<Graph> &source,
                               const Node<Graph> &target, CLOCK start) const
      -> std::shared_ptr<Segment>;
};
