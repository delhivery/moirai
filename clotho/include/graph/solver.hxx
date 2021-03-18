#ifndef GRAPH_SOLVER_HXX
#define GRAPH_SOLVER_HXX
#include "graph/structures.hxx"
#include "typedefs.hxx"
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/labeled_graph.hpp>
#include <boost/graph/named_function_params.hpp>
#include <boost/graph/visitors.hpp>
#include <boost/property_map/function_property_map.hpp>
#include <boost/property_map/property_map.hpp>
#include <string_view>
#include <type_traits>

namespace ambasta {
template<typename Vertex, typename Edge>
using Graph =
  boost::labeled_graph<boost::adjacency_list<boost::vecS,
                                             boost::vecS,
                                             boost::directedS,
                                             std::shared_ptr<Vertex>,
                                             std::shared_ptr<Edge>>,
                       std::string_view>;

template<class G>
using Vertex = typename boost::graph_traits<G>::vertex_descriptor;

template<class G>
using Edge = typename boost::graph_traits<G>::edge_descriptor;

template<typename V, typename E>
class BaseSolver
{
  static_assert(std::is_base_of<Node, V>::value,
                "Vertex must be derived from Node");
  static_assert(std::is_base_of<Route, E>::value,
                "Edge must be derived from Route");

private:
  typedef Graph<V, E> _Graph;
  typedef Edge<_Graph> EdgeDescriptor;
  typedef Vertex<_Graph> VertexDescriptor;

protected:
  _Graph m_graph;

  std::pair<std::shared_ptr<V>, bool> vertex(std::string_view label) const
  {
    auto vertex = boost::vertex_by_label(label, m_graph);

    if (vertex == boost::graph_traits<_Graph>::null_vertex()) {
      return { nullptr, false };
    }
    return { m_graph[vertex], true };
  }

  std::pair<std::shared_ptr<V>, bool> vertex(const VertexDescriptor& v) const
  {
    return { m_graph[v], true };
  }

  std::pair<std::shared_ptr<E>, bool> edge(const EdgeDescriptor& e) const
  {
    return { m_graph[e], true };
  }

  std::pair<Vertex<Graph<V, E>>, bool> add_vertex(std::shared_ptr<V> v)
  {
    boost::add_vertex(v->label(), v, m_graph);
  }

  std::pair<EdgeDescriptor, bool> add_edge(std::shared_ptr<V> u,
                                           std::shared_ptr<V> v,
                                           std::shared_ptr<E> ep)
  {
    return boost::add_edge(u, v, ep, m_graph);
  }

  std::pair<EdgeDescriptor, bool> add_edge(std::string_view u_label,
                                           std::string_view v_label,
                                           std::shared_ptr<E> ep)
  {
    return boost::add_edge_by_label(u_label, v_label, ep, m_graph);
  }

  void show() const;
};

template<typename V, typename E, Algorithm Alg>
class Solver : public BaseSolver<V, E>
{
  virtual void path(std::string_view,
                    std::string_view,
                    const TIMESTAMP&,
                    const bool) const = 0;
};
}

#endif
