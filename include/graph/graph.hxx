#include <cstddef>
#include <limits>
#include <memory>
#include <unordered_map>

template<class VertexT, class EdgeT>
class Graph
{
private:
  std::size_t max_node_index, max_edge_index;

  std::unordered_map<size_t, VertexT> vertex_map;
  std::unordered_map<size_t, EdgeT> edge_map;

  VertexT& operator[](const size_t& vertex_id)
  {
    return vertex_map.at(vertex_id);
  }

  const VertexT& operator[](const size_t& vertex_id) const
  {
    return vertex_map.at(vertex_id);
  }

  EdgeT& get_edge(const size_t& edge_id) { return edge_map.at(edge_id); }

public:
  Graph()
    : max_node_index(std::numeric_limits<size_t>::min())
    , max_edge_index(std::numeric_limits<size_t>::min())
  {}

  Graph(const Graph& other)
    : max_node_index(other.max_node_index)
    , max_edge_index(other.max_edge_index)
    , vertex_map(other.vertex_map)
    , edge_map(other.edge_map)
  {}

  static std::shared_ptr<Graph<VertexT, EdgeT>> get()
  {
    return std::make_shared<Graph<VertexT, EdgeT>>();
  }
};
