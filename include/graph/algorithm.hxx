#include <compare>
#include <functional>
#include <queue>
#include <ranges>
#include <unordered_map>
#include <utility>

template<class VertexIndexT, class DistanceT>
using WeightT = typename std::pair<VertexIndexT, DistanceT>;

template<class VertexIndexT, class DistanceT>
auto
operator<=>(const WeightT<VertexIndexT, DistanceT>& lhs,
            const WeightT<VertexIndexT, DistanceT>& rhs)
  -> decltype(lhs <=> rhs)
{
  using R = decltype(lhs <=> rhs);

  if (lhs.first < rhs.first)
    return R::less;

  if (lhs.first > rhs.first)
    return R::greater;

  if (lhs.second < rhs.second)
    return R::less;

  if (lhs.second > rhs.second)
    return R::greater;
  return R::equal;
}

template<class GraphT,
         template<typename GraphT::VertexIndexT, typename DistanceT>
         class DistanceMapT,
         typename DistanceT>
class BFS
{
private:
  typedef typename GraphT::VertexIndexT VertexIndexT;
  GraphT m_graph;
  std::priority_queue<WeightT<VertexIndexT, DistanceT>> m_priority_queue;
  VertexIndexT m_source, m_target;

public:
  BFS(const GraphT& graph)
    : m_graph(graph)
  {}
  // doBFS, source, true, !is_directed, -1, inf
  void search(
    const typename GraphT::VertexIndexT& source,
    const typename GraphT::VertexIndexT target,
    DistanceMapT<GraphT::VertexIndexT,
                 std::pair<DistanceT, typename GraphT::EdgeIndexT&>>& distances,
    const DistanceT& zero,
    const DistanceT& inf,
    std::function<const DistanceT&(typename GraphT::Edge, DistanceT)> weight)
  {
    static_assert(m_graph[source] != nullptr);

    distances.clear();
    distances.reserve(m_graph.size());
    std::for_each(std::ranges::views::iota(0, m_graph.size()),
                  [&distances, &inf](auto const& index) {
                    distances[index] = std::make_pair(inf, GraphT::null_edge());
                  });

    distances[source] = zero;

    m_priority_queue.clear();
    m_priority_queue.push(std::make_pair(source, zero));

    while (!m_priority_queue.empty()) {
      const VertexIndexT current = m_priority_queue.top();
      const DistanceT distance_current = distances[current];
      m_priority_queue.pop();

      if (distance_current == inf)
        break;

      const auto current_vertex_properties = m_graph[current];

      for (const auto [out_edge_index, out_edge] :
           m_graph.outgoing_edges(current)) {
        const DistanceT cost_edge = weight(out_edge, distance_current);
        VertexIndexT edge_target = m_graph.target(out_edge_index);

        if (distance_current + cost_edge < distances[out_edge_index].first) {
          distances[out_edge_index] =
            std::make_pair(distance_current + cost_edge, out_edge_index);

          m_priority_queue.emplace();
        }
      }
    }
  }
};

template<class GraphT,
         template<typename GraphT::VertexIndexT, typename DistanceT>
         class DistanceMapT,
         typename DistanceT>
void
get_shortest_path(const GraphT& graph,
                  const typename GraphT::VertexIndexT& source,
                  const typename GraphT::VertexIndexT& target,
                  DistanceMapT<GraphT::VertexIndexT, DistanceT>& distances,
                  const DistanceT& zero,
                  const DistanceT& inf)
{
  BFS<GraphT, DistanceMapT, DistanceT> bfs(graph);
  bfs.search(source, target, distances, zero, inf);
  distances.clear();
  distances.swap(bfs.distances);
}
