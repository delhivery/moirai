/** @file graph.hpp
 * @brief Defines base graph and helper functions
 */
#ifndef GRAPH_HPP_INCLUDED
#define GRAPH_HPP_INCLUDED

#include <iosfwd>      // for string
#include <limits>      // for numeric_limits
#include <stddef.h>    // for size_t
#include <string_view> // for string_view
#include <vector>      // for vector

#include <boost/graph/adjacency_list.hpp>  // for vecS (ptr only), adjacenc...
#include <boost/graph/graph_selectors.hpp> // for directedS
#include <boost/graph/graph_traits.hpp>    // for graph_traits, graph_trait...
#include <boost/pending/property.hpp>      // for no_property

const double P_D_INF = std::numeric_limits<double>::infinity();
const double N_D_INF = -1 * P_D_INF;

const long P_L_INF = 4102444800;

const long TIME_DURINAL = 24 * 60;

/**
 * @brief Structure representing a bundled properties of a vertex in a
 * graph/tree.
 */
struct VertexProperty {
  /**
   * @brief Unique human readeable name for vertex
   */
  std::string code;

  /**
   * @brief Alias used in common parlance
   */
  std::string name;

  /**
   * @brief Default constructs an empty vertex property.
   */
  VertexProperty() {}

  /**
   * @brief Constructs a vertex property
   * @param[in] : index of ther vertex in graph
   * @param[in] : unique human readable name for vertex
   */
  VertexProperty(std::string_view, std::string_view);
};

/**
 * @brief Structure representing a bundled properties of an edge in a
 * graph/tree.
 */
struct EdgeProperty {
  /**
   * @brief Flag to indicate connection as continuous or time-discrete
   */
  bool percon = false;

  /**
   * @brief Unique human readable name of the edge
   */
  std::string code;

  /**
   * @brief Actual departure time at source of edge
   */
  long _dep;

  /**
   * @brief Actual duration of traversal of edge
   */
  long _dur;

  /**
   * @brief Time to process inbound at edge destination
   */
  long _tip;

  /**
   * @brief Time to aggregate items for outbound via edge at edge source
   */
  long _tap;

  /**
   * @brief Time to process outbound via edge at edge source
   */
  long _top;

  /**
   * @brief Computed departure time at source of edge
   * @details Calculated generally as actual departure minus aggregation time
   * minus outbound time
   */
  long dep;

  /**
   * @brief Computed duration for traversal via edge
   * @details Calculated generally as actual duration plus aggregation plus
   * outbound plus inbound time
   */
  long dur;

  /**
   * @brief Default constructs an empty edge property.
   */
  EdgeProperty() {}

  /**
   * @brief Constructs an edge property for a continuous edge(such as custody
   * scan).
   * @details Continuous edges are free i.e. there is no physical cost
   * associated against movement via them.
   * @param[in] : Processing time in seconds for outbound at source vertex
   * @param[in] : Processing time in seconds for aggregation at source vertex
   * @param[in] : Processing time in seconds for inbound at destination vertex.
   * @param[in] : Unique human readable name for edge
   */
  EdgeProperty(const long, const long, const long, std::string_view);

  /**
   * @brief Constructs an edge property for a time-discrete edge
   * @details A time-discrete edge is an edge with a discrete start and duration
   * attribute
   * @param[in] : Time of departure from source vertex
   * @param[in] : Duration of iterating the edge
   * @param[in] : Processing time in seconds for outbound at source vertex
   * @param[in] : Processing time in seconds for aggregation at source vertex
   * @param[in] : Processing time in seconds for inbound at destination vertex.
   * @param[in] : Unique human readable name for edge
   */
  EdgeProperty(const long, const long, const long, const long, const long,
               std::string_view);

  /**
   * @brief Construct an edge property from another ( copy constructor) with a
   * new index
   */
  EdgeProperty(const size_t, EdgeProperty another);

  /**
   * @brief Calculate the wait time to traverse this edge.
   * @param[in] : Time of arrival at edge source.
   * @return Time spent waiting at edge source before departing via this edge
   */
  long wait_time(const long) const;

  /**
   * @brief Returns the Cost to traverse this edge.
   * @param[in] : Cost of arrival at edge source
   * @param[in] : Maximum time permissible to reach destination
   * @return Cost on traversing this edge.
   */
  long weight(const long, const long);
};

/**
 * @brief Structure representing a segment in the traversal of the graph/tree.
 */
struct Path {
  /**
   * @brief Source vertex in segment
   */
  std::string_view src;
  /**
   * @brief Edge used to traverse to destination vertex in segment
   */
  std::string_view conn;
  /**
   * @brief Destination vertex in segment
   */
  std::string_view dst;
  /**
   * @brief Time of arrival at source vertex
   */
  long arr;

  /**
   * @brief Minimum time by which item should arrive at source vertex
   */
  long mdep;

  /**
   * @brief Time of departure from source vertex
   */
  long dep;

  /**
   * @brief Default constructs an empty traversal of the graph
   */
  Path() {}

  /**
   * @brief Constructs a segment representing movement of an object.
   * @param[in] : Source Vertex
   * @param[in] : Edge used to reach destination from source
   * @param[in] : Destination Vertex
   * @param[in] : Arrival time at destination vertex
   * @param[in] : Minimum time of arrival at source vertex for succesful
   * departure
   * @param[in] : Departure time from source vertex
   * @param[in] : Cost incurred to arrive at source vertex
   */
  Path(std::string_view, std::string_view, std::string_view, long, long, long);
};

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS,
                              VertexProperty, EdgeProperty>
    Graph;
typedef boost::graph_traits<Graph>::vertex_descriptor Vertex;
typedef boost::graph_traits<Graph>::edge_descriptor Edge;
/**
 * @brief Interface representing the graph which can be traversed in different
 * ways to satisfy various constraints
 */
class BaseGraph {
protected:
  /**
   * @brief Actual graph, stored as a bgl::adjacency_list
   */
  Graph graph;

public:
  /**
   * @brief Default constructs an empty Graph
   */
  BaseGraph() {}

  /**
   * @brief Destroys the graph
   */
  virtual ~BaseGraph() {}

  /**
   * @brief Check if vertex exists in graph
   * @param[in] : Unique human readable name for the vertex
   */
  bool has_vertex(std::string_view) const;

  Vertex add_vertex(std::string_view) const;

  /**
   * @brief Adds a vertex to the graph
   * @param[in] : Unique human readable name for the vertex
   */
  Vertex add_vertex(std::string_view, std::string_view);

  /**
   * @brief Adds a continuous edge to the graph
   * @param[in] : Source vertex of edge
   * @param[in] : Destination vertex of edge
   * @param[in] : Unique human readable name for edge
   * @param[in] : Processing time in seconds for inbound at destination vertex.
   * @param[in] : Processing time in seconds for aggregation at source vertex
   * @param[in] : Processing time in seconds for outbound at source vertex
   */
  Edge add_edge(std::string_view, std::string_view, std::string_view,
                std::string_view, std::string_view, const long, const long,
                const long);
  /**
   * @brief Adds a continuous edge to the graph
   * @param[in] : Source vertex of edge
   * @param[in] : Destination vertex of edge
   * @param[in] : Unique human readable name for edge
   * @param[in] : Processing time in seconds for inbound at destination vertex.
   * @param[in] : Processing time in seconds for aggregation at source vertex
   * @param[in] : Processing time in seconds for outbound at source vertex
   */
  Edge add_edge(std::string_view, std::string_view, std::string_view,
                const long, const long, const long);

  /**
   * @brief Adds a continuous edge to the graph
   * @param[in] : Source vertex of edge
   * @param[in] : Destination vertex of edge
   * @param[in] : Unique human readable name for edge
   * @param[in] : Processing time in seconds for inbound at destination vertex.
   * @param[in] : Processing time in seconds for aggregation at source vertex
   * @param[in] : Processing time in seconds for outbound at source vertex
   */
  Edge add_edge(Vertex, Vertex, std::string_view, const long, const long,
                const long);

  /**
   * @brief Adds a discrete edge to the graph
   * @param[in] : Source vertex of edge
   * @param[in] : Destination vertex of edge
   * @param[in] : Unique human readable name for edge
   * @param[in] : Time of departure from source vertex
   * @param[in] : Duration of iterating the edge
   * @param[in] : Processing time in seconds for inbound at destination vertex.
   * @param[in] : Processing time in seconds for aggregation at source vertex
   * @param[in] : Processing time in seconds for outbound at source vertex
   */
  Edge add_edge(std::string_view, std::string_view, std::string_view,
                const long, const long, const long, const long, const long);

  /**
   * @brief Adds a discrete edge to the graph
   * @param[in] : Source vertex of edge
   * @param[in] : Destination vertex of edge
   * @param[in] : Unique human readable name for edge
   * @param[in] : Time of departure from source vertex
   * @param[in] : Duration of iterating the edge
   * @param[in] : Processing time in seconds for inbound at destination vertex.
   * @param[in] : Processing time in seconds for aggregation at source vertex
   * @param[in] : Processing time in seconds for outbound at source vertex
   */
  Edge add_edge(std::string_view, std::string_view, std::string_view,
                std::string_view, std::string_view, const long, const long,
                const long, const long, const long);
  /**
   * @brief Adds a discrete edge to the graph
   * @param[in] : Source vertex of edge
   * @param[in] : Destination vertex of edge
   * @param[in] : Unique human readable name for edge
   * @param[in] : Time of departure from source vertex
   * @param[in] : Duration of iterating the edge
   * @param[in] : Processing time in seconds for inbound at destination vertex.
   * @param[in] : Processing time in seconds for aggregation at source vertex
   * @param[in] : Processing time in seconds for outbound at source vertex
   */
  Edge add_edge(Vertex, Vertex, std::string_view, const long, const long,
                const long, const long, const long);

  /**
   * @brief Finds and returns a path based on various relaxation criteria
   * @param[in] : Source vertex
   * @param[in] : Destination vertex
   * @param[in] : Time of arrival at source vertex
   * @param[in] : Maximum time to arrive at destination vertex
   * @return A vector of Path representing an ideal path satisfying specified
   * constraints
   */
  virtual std::vector<Path> find_path(std::string_view, std::string_view, long,
                                      long) const = 0;

  std::string show() const;
};
#endif
