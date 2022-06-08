#ifndef MOIRAI_SOLVER_HXX
#define MOIRAI_SOLVER_HXX

#include "date_utils.hxx"     // for CLOCK
#include "transportation.hxx" // for Trans...
#include <algorithm>
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
#include <fmt/format.h>
#include <iostream> // for cout
#include <map>      // for map
#include <sstream>  // for strings...
#include <string>   // for strin...
#include <tuple>    // for tuple...
#include <utility>  // for pair
#include <variant>
#include <vector> // for vector

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

// typedef std::vector<std::tuple<std::shared_ptr<TransportCenter>,
//                               std::shared_ptr<TransportCenter>,
//                               std::shared_ptr<TransportEdge>,
//                               CLOCK>>
//  Path;

struct Segment
{
  std::shared_ptr<TransportCenter> m_node = nullptr;
  std::shared_ptr<TransportEdge> m_outbound = nullptr;
  CLOCK m_distance;
};

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

  std::string show() const;

  std::string show_all() const;

  template<typename FilteredGraph>
  std::vector<Segment> path_forward(const Node<FilteredGraph>& source,
                                    const Node<FilteredGraph>& target,
                                    CLOCK start,
                                    const FilteredGraph& filtered_graph) const
  {
    using node_t = Node<FilteredGraph>;
    using edge_t = Edge<FilteredGraph>;
    using pred_edge_map_t = std::map<node_t, edge_t>;
    using pred_edge_pmap_t = boost::associative_property_map<pred_edge_map_t>;

    pred_edge_map_t preds;
    pred_edge_pmap_t preds_pmap(preds);
    std::vector<CLOCK> distances(boost::num_vertices(filtered_graph));

    auto recorder =
      boost::record_edge_predecessors(preds_pmap, boost::on_edge_relaxed());
    boost::dijkstra_visitor<decltype(recorder)> visitor(recorder);

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
        .distance_compare([](CLOCK lhs, CLOCK rhs) { return lhs < rhs; })
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

    std::vector<Segment> path;
    path.emplace_back();

    for (node_t current = target; current != source;
         current = boost::source(preds_pmap[current], filtered_graph)) {
      auto edge_descriptor = preds_pmap[current];
      auto sour_descriptor = boost::source(edge_descriptor, filtered_graph);
      CLOCK distance = distances[current];

      if (distance == CLOCK::max())
        return path;

      path.back().m_node = filtered_graph[current];
      path.back().m_distance = distances[current];

      path.emplace_back();
      path.back().m_outbound = filtered_graph[edge_descriptor];
    }
    path.back().m_node = filtered_graph[source];
    path.back().m_distance = distances[source];
    std::reverse(path.begin(), path.end());
    return path;
  }

  template<typename FilteredGraph>
  std::vector<Segment> path_reverse(const Node<FilteredGraph>& source,
                                    const Node<FilteredGraph>& target,
                                    CLOCK start,
                                    const FilteredGraph& filtered_graph) const
  {
    using node_t = Node<FilteredGraph>;
    using edge_t = Edge<FilteredGraph>;
    using pred_edge_map_t = std::map<node_t, edge_t>;
    using pred_edge_pmap_t = boost::associative_property_map<pred_edge_map_t>;

    pred_edge_map_t preds;
    pred_edge_pmap_t preds_pmap(preds);
    std::vector<CLOCK> distances(boost::num_vertices(filtered_graph));

    auto recorder =
      boost::record_edge_predecessors(preds_pmap, boost::on_edge_relaxed());
    boost::dijkstra_visitor<decltype(recorder)> visitor(recorder);

    auto w_map = boost::make_transform_value_property_map(
      [](std::shared_ptr<TransportEdge> edge) {
        return edge->weight<PathTraversalMode::REVERSE>();
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

    std::vector<Segment> path;
    path.emplace_back();

    for (node_t current = target; current != source;
         current = boost::source(preds_pmap[current], filtered_graph)) {
      auto edge_descriptor = preds_pmap[current];
      auto sour_descriptor = boost::source(edge_descriptor, filtered_graph);
      CLOCK distance = distances[current];

      if (distance == CLOCK::min())
        return path;

      path.back().m_node = filtered_graph[current];
      path.back().m_distance = distances[current];
      path.back().m_outbound = filtered_graph[edge_descriptor];

      path.emplace_back();

      /*
      if (segment->outbound->m_movement == MovementType::CARTING)
        segment->distance +=
          segment->node
            ->get_latency<MovementType::CARTING, ProcessType::OUTBOUND>();
      else
        segment->distance +=
          segment->node
            ->get_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>();
      */
    }
    path.back().m_node = filtered_graph[source];
    path.back().m_distance = distances[source];
    return path;
  }

  template<PathTraversalMode P, VehicleType V = VehicleType::AIR>
  std::vector<Segment> find_path(const Node<Graph>&,
                                 const Node<Graph>&,
                                 CLOCK) const;
};

#endif
