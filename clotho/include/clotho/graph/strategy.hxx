#ifndef GRAPH_STRATEGY_HXX
#define GRAPH_STRATEGY_HXX
/*
#include "graph/labeled_graph.hxx"
#include "graph/structures.hxx"
#include "typedefs.hxx"
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/named_function_params.hpp>
#include <boost/graph/visitors.hpp>
#include <boost/property_map/function_property_map.hpp>
#include <boost/property_map/property_map.hpp>
#include <string_view>
#include <type_traits>
*/

#include <clotho/boost/graph/labeled_graph.hxx>
#include <clotho/graph/typedefs.hxx>
#include <memory>
#include <string_view>

namespace ambasta {
typedef boost::labeled_graph<boost::adjacency_list<boost::vecS,
                                                   boost::vecS,
                                                   boost::directedS,
                                                   std::shared_ptr<Node>,
                                                   std::shared_ptr<Route>>,
                             std::string_view>
  Graph;

typedef boost::graph_traits<Graph>::vertex_descriptor VertexDescriptor;

typedef boost::graph_traits<Graph>::edge_descriptor EdgeDescriptor;

class Strategy
{
protected:
  std::shared_ptr<Graph> m_graph = nullptr;

  std::pair<const std::shared_ptr<Node>, bool> vertex(std::string_view) const;

  std::pair<const std::shared_ptr<Node>, bool> vertex(
    const VertexDescriptor&) const;

  std::pair<const std::shared_ptr<Route>, bool> edge(
    const EdgeDescriptor&) const;

  std::pair<const VertexDescriptor, bool> add_vertex(std::shared_ptr<Node>);

  std::pair<const EdgeDescriptor, bool> add_edge(std::string_view,
                                                 std::string_view,
                                                 std::shared_ptr<Route>);

  std::pair<const EdgeDescriptor, bool> add_edge(std::shared_ptr<Node>,
                                                 std::shared_ptr<Node>,
                                                 std::shared_ptr<Route>);

  void show() const;

public:
  Strategy();

  Strategy(std::shared_ptr<Graph>);

  virtual ~Strategy();

  virtual const std::tuple<TIME_OF_DAY, MINUTES, LEVY> weight(
    const EdgeDescriptor&) const = 0;

  virtual COST zero(const TIMESTAMP = TIMESTAMP::min(),
                    const LEVY = std::numeric_limits<LEVY>::min()) const = 0;

  virtual COST inf(const TIMESTAMP = TIMESTAMP::max(),
                   const LEVY = std::numeric_limits<LEVY>::max()) const = 0;

  virtual bool compare(const COST&, const COST&) const = 0;

  virtual COST combine(const COST&,
                       const std::tuple<TIME_OF_DAY, MINUTES, LEVY>&) const = 0;

  void solve(std::string_view,
             std::string_view,
             const TIMESTAMP&,
             const bool) const;

  /*
  void solve(std::string_view,
             std::string_view,
             const TIMESTAMP&,
             const bool,
             const std::pair<TIMESTAMP, LEVY>&) const;
  */
};

}

#endif
