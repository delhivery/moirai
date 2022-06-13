#ifndef MOIRAI_SOLVER_HXX
#define MOIRAI_SOLVER_HXX

#include "date_utils.hxx"
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

using namespace boost;

using Graph = adjacency_list<vecS,
                             vecS,
                             bidirectionalS,
                             std::shared_ptr<TransportCenter>,
                             std::shared_ptr<TransportEdge>>;

template<class G>
using Node = typename graph_traits<G>::vertex_descriptor;

template<class G>
using Edge = typename graph_traits<G>::edge_descriptor;

class Segment
{
private:
  std::shared_ptr<TransportCenter> mNode = nullptr;
  std::shared_ptr<TransportEdge> mOutbound = nullptr;
  datetime mDistance;

public:
  Segment(std::shared_ptr<TransportCenter>,
          std::shared_ptr<TransportEdge>,
          datetime);

  [[nodiscard]] auto distance() const -> datetime;
};

class Solver
{

private:
  Graph graph;
  std::map<std::string, Node<Graph>> mNamedVertexMap;
  std::map<std::string, Edge<Graph>> mNamedEdgeMap;

public:
  [[nodiscard]] auto add_node(const std::string&) const
    -> std::pair<Node<Graph>, bool>;

  [[nodiscard]] auto add_node(const std::shared_ptr<TransportCenter>&)
    -> std::pair<Node<Graph>, bool>;

  [[nodiscard]] auto get_node(Node<Graph>) const
    -> std::shared_ptr<TransportCenter>;

  [[nodiscard]] auto add_edge(const std::string&) const
    -> std::pair<Edge<Graph>, bool>;

  [[nodiscard]] auto add_edge(const Node<Graph>&,
                              const Node<Graph>&,
                              const std::shared_ptr<TransportEdge>&)
    -> std::pair<Edge<Graph>, bool>;

  template<typename FilteredGraph>
  auto solve(const Node<FilteredGraph>&,
             const Node<FilteredGraph>&,
             datetime,
             datetime,
             const auto&,
             const auto&,
             const FilteredGraph&) const -> std::vector<Segment>;

  template<PathTraversalMode P, VehicleType V = VehicleType::AIR>
  [[nodiscard]] auto find_path(const Node<Graph>&,
                               const Node<Graph>&,
                               datetime) const -> std::vector<Segment>;
};

#endif
