#include "date_utils.hxx"                                      // for CLOCK
#include "transportation.hxx"                                  // for Trans...
#include <boost/graph/adjacency_list.hpp>                      // for source
#include <boost/graph/dijkstra_shortest_paths.hpp>             // for dijks...
#include <boost/graph/filtered_graph.hpp>                      // for target
#include <boost/graph/graph_selectors.hpp>                     // for bidir...
#include <boost/graph/graph_traits.hpp>                        // for graph...
#include <boost/graph/named_function_params.hpp>               // for prede...
#include <boost/graph/properties.hpp>                          // for verte...
#include <boost/pending/property.hpp>                          // for edge_...
#include <boost/property_map/property_map.hpp>                 // for make_...
#include <boost/property_map/transform_value_property_map.hpp> // for make_...
#include <chrono>                                              // for opera...
#include <compare>                                             // for opera...
#include <map>                                                 // for map
#include <string_view>                                         // for strin...
#include <utility>                                             // for pair
#include <vector>                                              // for vector

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

class Solver
{
private:
  Graph graph;
  std::map<std::string_view, Node<Graph>> vertex_by_name;
  std::map<std::string_view, Edge<Graph>> edge_by_name;

public:
  std::pair<Node<Graph>, bool> add_node(std::string_view) const;

  std::pair<Node<Graph>, bool> add_node(const TransportCenter&);

  std::pair<Edge<Graph>, bool> add_edge(std::string_view) const;

  std::pair<Edge<Graph>, bool> add_edge(const Node<Graph>&,
                                        const Node<Graph>&,
                                        const TransportEdge&);

  template<typename FilteredGraph>
  void path_forward(const Node<Graph>& source,
                    CLOCK start,
                    const FilteredGraph& filtered_graph) const
  {

    std::vector<Node<FilteredGraph>> predecessors(
      boost::num_vertices(filtered_graph));
    auto predecessor_map = boost::make_iterator_property_map(
      predecessors.begin(), boost::get(boost::vertex_index, filtered_graph));

    std::vector<CLOCK> distances(boost::num_vertices(filtered_graph));
    auto distance_map = boost::make_iterator_property_map(
      distances.begin(), boost::get(boost::vertex_index, filtered_graph));

    CLOCK zero = CLOCK::min();
    CLOCK infi = CLOCK::max();

    auto combine = []<typename BinaryFunction>(CLOCK start,
                                               BinaryFunction f) -> CLOCK {
      return f(start);
    };

    auto w_map = boost::make_transform_value_property_map(
      [](const TransportEdge& edge) {
        return edge.weight<PathTraversalMode::FORWARD>();
      },
      get(boost::edge_bundle, filtered_graph));

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
        .distance_inf(CLOCK::max()));
  }

  template<typename FilteredGraph>
  void path_reverse(const Node<Graph>& source,
                    CLOCK start,
                    const FilteredGraph& filtered_graph) const
  {

    std::vector<Node<FilteredGraph>> predecessors(
      boost::num_vertices(filtered_graph));
    auto predecessor_map = boost::make_iterator_property_map(
      predecessors.begin(), boost::get(boost::vertex_index, filtered_graph));

    std::vector<CLOCK> distances(boost::num_vertices(filtered_graph));
    auto distance_map = boost::make_iterator_property_map(
      distances.begin(), boost::get(boost::vertex_index, filtered_graph));

    CLOCK zero = CLOCK::min();
    CLOCK infi = CLOCK::max();

    auto combine = []<typename BinaryFunction>(CLOCK start,
                                               BinaryFunction f) -> CLOCK {
      return f(start);
    };

    auto w_map = boost::make_transform_value_property_map(
      [](const TransportEdge& edge) {
        return edge.weight<PathTraversalMode::FORWARD>();
      },
      get(boost::edge_bundle, filtered_graph));

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
        .distance_inf(CLOCK::max()));
  }

  template<PathTraversalMode P, VehicleType V = VehicleType::AIR>
  void operator()(const Node<Graph>&, CLOCK) const;
};
