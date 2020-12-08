/** @file optimal.hpp
 * @brief Defines a single objective path solver.
 * @details Defines a graph iterator to find a path from a source to destination
 * while optimizing on a singular value without any additional constraints
 * (least time/cost) etc.
 */
#ifndef OPTIMAL_HPP_INCLUDED
#define OPTIMAL_HPP_INCLUDED

#include <string_view> // for string_view
#include <utility>     // for pair
#include <vector>      // for vector

#include <boost/graph/graph_traits.hpp> // for graph_traits, graph_traits<>...

#include "graph.hxx"

typedef std::vector<long> DistanceMap;
typedef std::vector<Edge> PredecessorMap;

typedef typename boost::graph_traits<Graph>::out_edge_iterator OutEdgesIterator;

/**
 * @brief Comparison operator to compare a pair of vertices and their associated
 * costs
 */
struct Compare {
public:
  /**
   * @brief Function operator to compare pair of vertices and their associated
   * costs
   * @param[in] : First pair of Vertex with associated Cost
   * @param[in] : Second pair of Vertex with associated Cost
   * @return True if first < second else false
   */
  bool operator()(const std::pair<Vertex, long> &,
                  const std::pair<Vertex, long> &) const;
};

/**
 * @brief Extends BaseGraph to implement a single criteria path optimization
 */
class Optimal : public BaseGraph {
private:
  /**
   * @brief Actual implementation of the path finding algorithm as a dijkstra
   * @param[in] :         Source vertex
   * @param[in] :         Destination vertex
   * @param[in,out] :     Map of vertices to their distances from source
   * @param[in, out] :    Map of vertices to their predecessor when iterating
   * from source
   * @param[in] :         Infinite Cost
   * @param[in] :         Zero/Base Cost
   * @param[in] :         Maximum duration by which the destination vertex must
   * be reached
   */
  void run_dijkstra(Vertex, Vertex, DistanceMap &, PredecessorMap &, long, long,
                    long) const;

public:
  /**
   * @brief Implementation of path finder declared in BaseGraph
   * @param[in] : Name of source vertex
   * @param[in] : Name of destination vertex
   * @param[in] : Time of arrival at source vertex
   * @param[in] : Time limit by which destination vertex needs to be arrived at
   */
  std::vector<Path> find_path(std::string_view, std::string_view, long,
                              long) const;
};

#endif
