#ifndef GRAPH_SOLVER_HPP
#define GRAPH_SOLVER_HPP
#include "typedefs.hxx"
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/labeled_graph.hpp>
#include <string_view>

namespace ambasta {
template<typename Vertex, typename Edge>
using Graph =
  boost::labeled_graph<boost::adjacency_list<boost::vecS,
                                             boost::vecS,
                                             boost::directedS,
                                             std::shared_ptr<Vertex>,
                                             std::shared_ptr<Edge>>,
                       std::string_view>;

template<class Graph>
using Vertex = typename boost::graph_traits<Graph>::vertex_descriptor;

template<class Graph>
using Edge = typename boost::graph_traits<Graph>::edge_descriptor;

template<typename V, typename E>
class BaseSolver
{
private:
  Graph<V, E> graph;

protected:
  std::pair<std::shared_ptr<V>, bool> vertex(std::string_view label) const
  {
    auto vertex = boost::vertex_by_label(label, graph);

    if (vertex == boost::graph_traits<Graph<V, E>>::null_vertex()) {
      return { nullptr, false };
    }
    return { graph[vertex], true };
  }

  std::pair<std::shared_ptr<V>, bool> vertex(const Vertex<Graph<V, E>>& v) const
  {
    return { graph[v], true };
  }

  std::pair<std::shared_ptr<E>, bool> edge(const Edge<Graph<V, E>>& e) const
  {
    return { graph[e], true };
  }

  std::pair<Vertex<Graph<V, E>>, bool> add_vertex(std::shared_ptr<V> v)
  {
    boost::add_vertex(v->label(), v, graph);
  }

  std::pair<Edge<Graph<V, E>>, bool> add_edge(std::shared_ptr<V> u,
                                              std::shared_ptr<V> v,
                                              std::shared_ptr<E> ep)
  {
    boost::add_edge(u, v, ep, graph);
  }

  std::pair<Edge<Graph<V, E>>, bool> add_edge(std::string_view u_label,
                                              std::string_view v_label,
                                              std::shared_ptr<E> ep)
  {
    boost::add_edge_by_label(u_label, v_label, ep, graph);
  }

  void show() const;

  void path(std::string_view u_label,
            std::string_view v_label,
            TIMESTAMP start,
            const bool restricted) const
  {
    auto predicate = [&restricted](auto const& e) -> bool {
      return !(restricted >> e->restricted);
    };
  }
};
}

#endif
