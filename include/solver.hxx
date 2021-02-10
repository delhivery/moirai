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
#include <compare>                                             // for opera...
#include <date/date.h>                                         // for date...
#include <fmt/core.h>                                          // for format...
#include <iostream>                                            // for cout
#include <map>                                                 // for map
#include <sstream>                                             // for strings...
#include <string>                                              // for strin...
#include <tuple>                                               // for tuple...
#include <utility>                                             // for pair
#include <vector>                                              // for vector

typedef boost::adjacency_list<boost::vecS,
                              boost::vecS,
                              boost::bidirectionalS,
                              std::shared_ptr<TransportCenter>,
                              std::shared_ptr<TransportEdge>>
  Graph;

template<class G>
using Node = typename boost::graph_traits<G>::vertex_descriptor;

template<class G>
using Edge = typename boost::graph_traits<G>::edge_descriptor;

typedef std::vector<std::tuple<std::shared_ptr<TransportCenter>,
                               std::shared_ptr<TransportCenter>,
                               std::shared_ptr<TransportEdge>,
                               CLOCK>>
  Path;

class Solver
{

private:
  Graph graph;
  std::map<std::string, Node<Graph>> vertex_by_name;
  std::map<std::string, Edge<Graph>> edge_by_name;

public:
  std::pair<Node<Graph>, bool> add_node(std::string) const;

  std::pair<Node<Graph>, bool> add_node(std::shared_ptr<TransportCenter>);

  std::shared_ptr<TransportCenter> get_node(const Node<Graph>) const;

  std::pair<Edge<Graph>, bool> add_edge(std::string) const;

  std::pair<Edge<Graph>, bool> add_edge(const Node<Graph>&,
                                        const Node<Graph>&,
                                        std::shared_ptr<TransportEdge>);

  template<typename FilteredGraph>
  Path path_forward(const Node<FilteredGraph>& source,
                    const Node<FilteredGraph>& target,
                    CLOCK start,
                    const FilteredGraph& filtered_graph) const
  {
    typedef std::map<Node<FilteredGraph>, Edge<FilteredGraph>>
      predecessor_edge_map_t;
    typedef boost::associative_property_map<predecessor_edge_map_t>
      predecessor_edge_property_map_t;

    predecessor_edge_map_t predecessors;
    predecessor_edge_property_map_t predecessors_property_map(predecessors);

    auto recorder = boost::record_edge_predecessors(predecessors_property_map,
                                                    boost::on_edge_relaxed());
    boost::dijkstra_visitor<decltype(recorder)> visitor(recorder);

    std::vector<CLOCK> distances(boost::num_vertices(filtered_graph));

    auto w_map = boost::make_transform_value_property_map(
      [](std::shared_ptr<TransportEdge> edge) {
        return edge->weight<PathTraversalMode::FORWARD>();
      },
      get(boost::edge_bundle, filtered_graph));

    boost::dijkstra_shortest_paths(
      filtered_graph,
      source,
      boost::distance_map(&distances[0])
        .weight_map(w_map)
        .distance_compare([](CLOCK lhs, CLOCK rhs) {
          /*
          std::cout << fmt::format("Comparing old {} and new {}: {}",
                                   lhs.time_since_epoch().count() * 60,
                                   rhs.time_since_epoch().count() * 60,
                                   lhs > rhs)
                    << std::endl;
          */
          return lhs < rhs;
        })
        .distance_combine([](CLOCK initial, COST cost) {
          CalcualateTraversalCost calculator;
          CLOCK computed =
            calculator.template operator()<PathTraversalMode::FORWARD>(initial,
                                                                       cost);
          if (computed < initial) {
            std::cout << fmt::format(
                           "Found a lower cost {} from initial {}. Cost: {},{}",
                           date::format("%D %T", computed),
                           date::format("%D %T", initial),
                           cost.first.count(),
                           cost.second.count())
                      << std::endl;
          }
          return computed;
        })
        .distance_zero(start)
        .distance_inf(CLOCK::max())
        .visitor(visitor));

    Path path;

    for (Node<FilteredGraph> current = target; current != source;
         current =
           boost::source(predecessors_property_map[current], filtered_graph)) {
      auto edge_descriptor = predecessors_property_map[current];
      auto sour_descriptor = boost::source(edge_descriptor, filtered_graph);
      CLOCK distance = distances[current];

      if (distance == CLOCK::max())
        break;
      auto conn = filtered_graph[edge_descriptor];
      auto sour = filtered_graph[sour_descriptor];
      auto targ = filtered_graph[current];
      path.push_back(std::make_tuple(sour, targ, conn, distance));
    }

    return path;
  }

  template<typename FilteredGraph>
  Path path_reverse(const Node<FilteredGraph>& source,
                    const Node<FilteredGraph>& target,
                    CLOCK start,
                    const FilteredGraph& filtered_graph) const
  {
    typedef std::map<Node<FilteredGraph>, Edge<FilteredGraph>>
      predecessor_edge_map_t;
    typedef boost::associative_property_map<predecessor_edge_map_t>
      predecessor_edge_property_map_t;

    predecessor_edge_map_t predecessors;
    predecessor_edge_property_map_t predecessors_property_map(predecessors);

    auto recorder = boost::record_edge_predecessors(predecessors_property_map,
                                                    boost::on_edge_relaxed());
    boost::dijkstra_visitor<decltype(recorder)> visitor(recorder);

    std::vector<CLOCK> distances(boost::num_vertices(filtered_graph));

    auto w_map = boost::make_transform_value_property_map(
      [](std::shared_ptr<TransportEdge> edge) {
        return edge->weight<PathTraversalMode::FORWARD>();
      },
      get(boost::edge_bundle, filtered_graph));

    boost::dijkstra_shortest_paths(
      filtered_graph,
      source,
      boost::distance_map(&distances[0])
        .weight_map(w_map)
        .distance_compare([](CLOCK lhs, CLOCK rhs) { return lhs > rhs; })
        .distance_combine([](CLOCK start, COST cost) {
          CalcualateTraversalCost calculator;
          return calculator.template operator()<PathTraversalMode::REVERSE>(
            start, cost);
        })
        .distance_zero(start)
        .distance_inf(CLOCK::min())
        .visitor(visitor));

    Path path;

    for (Node<FilteredGraph> current = target; current != source;
         current =
           boost::source(predecessors_property_map[current], filtered_graph)) {
      auto edge_descriptor = predecessors_property_map[current];
      auto sour_descriptor = boost::source(edge_descriptor, filtered_graph);
      CLOCK distance = distances[current];

      if (distance == CLOCK::min())
        break;
      auto conn = filtered_graph[edge_descriptor];
      auto sour = filtered_graph[sour_descriptor];
      auto targ = filtered_graph[current];
      path.push_back(std::make_tuple(sour, targ, conn, distance));
    }
    return path;
  }

  template<PathTraversalMode P, VehicleType V = VehicleType::AIR>
  Path find_path(const Node<Graph>&, const Node<Graph>&, CLOCK) const;
};
