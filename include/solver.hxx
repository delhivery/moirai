#ifndef MOIRAI_SOLVER_HXX
#define MOIRAI_SOLVER_HXX

#include "transportation.hxx"
#include <boost/graph/adjacency_list.hpp>

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
