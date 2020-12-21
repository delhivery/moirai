#include "date_utils.hxx"                          // for CLOCK
#include "transportation.hxx"                      // for Trans...
#include <boost/graph/adjacency_list.hpp>          // for source
#include <boost/graph/dijkstra_shortest_paths.hpp> // for dijks...
#include <boost/graph/filtered_graph.hpp>          // for target
#include <boost/graph/graph_selectors.hpp>         // for bidir...
#include <boost/graph/graph_traits.hpp>            // for graph...
#include <boost/graph/named_function_params.hpp>   // for prede...
#include <boost/graph/properties.hpp>              // for verte...
#include <boost/graph/visitors.hpp>
#include <boost/pending/property.hpp>                          // for edge_...
#include <boost/property_map/property_map.hpp>                 // for make_...
#include <boost/property_map/transform_value_property_map.hpp> // for make_...
#include <chrono>                                              // for opera...
#include <compare>                                             // for opera...
#include <date/date.h>
#include <fmt/core.h>
#include <iostream>
#include <locale>
#include <map> // for map
#include <sstream>
#include <string> // for strin...
#include <tuple>
#include <utility> // for pair
#include <vector>  // for vector

typedef boost::adjacency_list<boost::vecS,
                              boost::vecS,
                              boost::bidirectionalS,
                              TransportCenter,
                              TransportEdge>
  Graph;

template<class G>
using Node = typename boost::graph_traits<G>::vertex_descriptor;

template<class G>
using Edge = typename boost::graph_traits<G>::edge_descriptor;

typedef std::vector<
  std::tuple<std::string, std::string, std::string, std::string, std::string>>
  Path;

class Solver
{

private:
  Graph graph;
  std::map<std::string, Node<Graph>> vertex_by_name;
  std::map<std::string, Edge<Graph>> edge_by_name;

public:
  std::pair<Node<Graph>, bool> add_node(std::string) const;

  std::pair<Node<Graph>, bool> add_node(const TransportCenter&);

  std::pair<Edge<Graph>, bool> add_edge(std::string) const;

  std::pair<Edge<Graph>, bool> add_edge(const Node<Graph>&,
                                        const Node<Graph>&,
                                        const TransportEdge&);

  template<typename FilteredGraph>
  Path path_forward(const Node<FilteredGraph>& source,
                    const Node<FilteredGraph>& target,
                    CLOCK start,
                    const FilteredGraph& filtered_graph) const
  {

    auto edges = boost::out_edges(source, filtered_graph);

    for (auto eiter = edges.first; eiter != edges.second; eiter++) {
      auto ed = filtered_graph[*eiter];
      std::cout << fmt::format("{}: {}", ed.name, ed.code) << std::endl;
    }

    std::vector<Node<FilteredGraph>> predecessors(
      boost::num_vertices(filtered_graph));
    std::vector<CLOCK> distances(boost::num_vertices(filtered_graph));

    auto w_map = boost::make_transform_value_property_map(
      [](const TransportEdge& edge) {
        return edge.weight<PathTraversalMode::FORWARD>();
      },
      get(boost::edge_bundle, filtered_graph));

    boost::dijkstra_shortest_paths(
      filtered_graph,
      source,
      boost::predecessor_map(
        boost::make_iterator_property_map(
          predecessors.begin(), get(boost::vertex_index, filtered_graph)))
        .distance_map(&distances[0])
        .weight_map(w_map)
        .distance_compare([](CLOCK lhs, CLOCK rhs) {
          std::cout << fmt::format("Comparing {} and {}",
                                   date::format("%D %T", lhs),
                                   date::format("%D %T", rhs))
                    << std::endl;
          return lhs < rhs;
        })
        .distance_combine([](CLOCK start, COST cost) {
          CalcualateTraversalCost calculator;
          return calculator.template operator()<PathTraversalMode::FORWARD>(
            start, cost);
        })
        .distance_zero(start)
        .distance_inf(CLOCK::max()));

    std::cout << "BGL djk over" << std::endl;

    Path path;

    std::cout << "Distance to source "
              << date::format("%D %T", distances[source]) << std::endl;

    for (Node<FilteredGraph> current = target; current != source;
         current = predecessors[current]) {
      auto node = filtered_graph[current];
      // auto edge = filtered_graph[predecessor_property_map[current]];
      CLOCK distance = distances[current];

      if (distance == CLOCK::max()) {
        std::cout << "No feasible paths" << std::endl;
        break;
      }
      path.push_back(std::make_tuple(
        node.code, node.name, "", "", date::format("%D %T", distance)));
      std::cout << fmt::format(
                     "{}: {}", node.name, date::format("%D %T", distance))
                << std::endl;
    }

    return path;
  }

  template<typename FilteredGraph>
  Path path_reverse(const Node<Graph>& source,
                    const Node<Graph>& target,
                    CLOCK start,
                    const FilteredGraph& filtered_graph) const
  {

    std::vector<Node<FilteredGraph>> predecessors(
      boost::num_vertices(filtered_graph));
    std::vector<CLOCK> distances(boost::num_vertices(filtered_graph));

    auto w_map = boost::make_transform_value_property_map(
      [](const TransportEdge& edge) {
        return edge.weight<PathTraversalMode::FORWARD>();
      },
      get(boost::edge_bundle, filtered_graph));

    typedef std::map<
      typename boost::graph_traits<FilteredGraph>::vertex_descriptor,
      typename boost::graph_traits<FilteredGraph>::edge_descriptor>
      predecessor_edge_map_t;
    typedef boost::associative_property_map<predecessor_edge_map_t>
      predecessor_edge_property_map_t;

    predecessor_edge_map_t predecessor_map;
    predecessor_edge_property_map_t predecessor_property_map(predecessor_map);

    auto recorder = boost::record_edge_predecessors(predecessor_property_map,
                                                    boost::on_edge_relaxed());

    boost::dijkstra_visitor<decltype(recorder)> visitor(recorder);

    boost::dijkstra_shortest_paths(
      filtered_graph,
      source,
      boost::predecessor_map(&predecessors[0])
        .distance_map(&distances[0])
        .weight_map(w_map)
        .distance_compare([](CLOCK lhs, CLOCK rhs) { return lhs < rhs; })
        .distance_combine([](CLOCK start, COST cost) {
          CalcualateTraversalCost calculator;
          return calculator.template operator()<PathTraversalMode::FORWARD>(
            start, cost);
        })
        .distance_zero(CLOCK::min())
        .distance_inf(CLOCK::max())
        .visitor(visitor));

    Path path;

    for (Node<Graph> current = target; current != source;
         current =
           boost::source(predecessor_property_map[current], filtered_graph)) {
      auto node = filtered_graph[current];
      auto edge = filtered_graph[predecessor_property_map[current]];
      CLOCK distance = distances[current];
      path.push_back(std::make_tuple(node.code,
                                     node.name,
                                     edge.code,
                                     edge.name,
                                     date::format("%D %T", distance)));
    }

    return path;
  }

  template<PathTraversalMode P, VehicleType V = VehicleType::AIR>
  Path find_path(const Node<Graph>&, const Node<Graph>&, CLOCK) const;

  std::string show()
  {
    return fmt::format(
      "Graph<{}, {}>", boost::num_vertices(graph), boost::num_edges(graph));
  }

  std::string vertices()
  {
    std::string tst;
    for (auto& vertex : vertex_by_name) {
      tst = fmt::format("{}, {}", tst, vertex.first);
    }
    return tst;
  }
};
