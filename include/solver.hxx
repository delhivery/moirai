#include "transportation.hxx"
#include <boost/graph/adjacency_list.hpp>
#include <map>
#include <string_view>

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

  template<PathTraversalMode P, VehicleType V=VehicleType::AIR>
  void operator()(const Node<Graph>&, CLOCK) const;
};
