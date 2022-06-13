#include <boost/config.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/limits.hpp>
#include <boost/graph/visitors.hpp>

template<class DistanceMap>
struct show_graph : public boost::base_visitor<show_graph<DistanceMap>>
{
  std::shared_ptr<DistanceMap> distance_map;

  show_graph(std::shared_ptr<DistanceMap> distance_map)
    : distance_map(distance_map)
  {}

  template<class Edge, class Graph>
  void operator()(Edge edge, const Graph& graph)
  {
    auto source = boost::source(edge, graph);
    auto target = boost::target(edge, graph);
    auto sourceDistance = boost::get(*distance_map, source);
    auto targetDistance = boost::get(*distance_map, target);
  }
};
