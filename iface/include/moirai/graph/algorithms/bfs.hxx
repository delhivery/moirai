#ifndef GRAPH_ALGORITHMS_BFS
#define GRAPH_ALGORITHMS_BFS
#include "moirai/property_maps/helpers.hxx"
#include <functional>
#include <moirai/graph/concepts.hxx>
#include <moirai/graph/visitors.hxx>
#include <moirai/graph/visitors/concepts.hxx>
#include <moirai/property_maps/concepts.hxx>
#include <queue>
#include <ranges>
#include <vector>

template <IncidenceGraphConcept GraphT, typename BufferT, typename VisitorT,
          ReadWritePropertyMapConcept ColorMapT, typename SourceIteratorT>
requires VisitorConcept<VisitorT, GraphT>
void bfs_visit(const GraphT &graph, SourceIteratorT source,
               SourceIteratorT target, BufferT &queue, VisitorT visitor,
               ColorMapT color_map) {
  using Vertex = typename GraphT::vertex_descriptor;
  using Color = typename ColorMapT::value_type;
  using OutEdgeIterator = typename GraphT::out_edge_iterator;

  for (; source != target; ++source) {
    Vertex vertex_source = *source;
    put(color_map, vertex_source, Color::gray());
    visitor.discover_vertex(vertex_source, graph);
    queue.push(vertex_source);
  }

  while (not queue.empty()) {
    Vertex current = queue.top();
    queue.pop();
    visitor.examine_vertex(current, graph);

    for (auto edge : graph.out_edges(current)) {
      Vertex edge_target = graph.target(*edge);
      visitor.examine_edge(*edge, graph);
      Color vertex_color = get(color_map, edge_target);

      if (vertex_color == Color::white()) {
        visitor.tree_edge(*edge, graph);
        put(color_map, edge_target, Color::gray());
        visitor.discover_vertex(edge_target, graph);
        queue.push(edge_target);
      } else {
        visitor.non_tree_edge(*edge, graph);

        if (vertex_color == Color::gray())
          visitor.gray_target(*edge, graph);
        else
          visitor.black_target(*edge, graph);
      }
      put(color_map, current, Color::black());
      visitor.finish_vertex(current, graph);
    }
  }
}

template <IncidenceGraphConcept GraphT, typename SourceIteratorT,
          typename BufferT, typename VisitorT, typename ColorMapT>
void bfs_visit(const GraphT &graph, typename GraphT::vertex_descriptor source,
               BufferT &queue, VisitorT visitor, ColorMapT color_map) {
  typename GraphT::vertex_descriptor sources[1] = {source};
  bfs_visit(graph, sources, sources + 1, queue, visitor, color_map);
}

template <VertexListGraphConcept GraphT, typename SourceIteratorT,
          typename BufferT, typename VisitorT, typename ColorMapT>
void bfs(const GraphT &graph, SourceIteratorT source, SourceIteratorT target,
         BufferT &queue, VisitorT visitor, ColorMapT color_map) {
  using Color = typename ColorMapT::value_type;

  for (auto vertex : graph.vertices()) {
    visitor.initialize_vertex(*vertex, graph);
    put(color_map, *vertex, Color::white());
  }
  bfs_visit(graph, source, target, queue, visitor, color_map);
}

template <VertexListGraphConcept GraphT, typename BufferT, typename VisitorT,
          typename ColorMapT>
void bfs(const GraphT &graph, typename GraphT::vertex_descriptor source,
         BufferT &queue, VisitorT visitor, ColorMapT color_map) {
  typename GraphT::vertex_descriptor sources[1] = {source};
  bfs(graph, sources, sources + 1, queue, visitor, color_map);
}

template <std::ranges::input_range VisitorsT> class BFSVisitor {
protected:
  VisitorsT m_visitors;

public:
  BFSVisitor() {}

  BFSVisitor(VisitorsT visitors) : m_visitors(visitors) {}

private:
  template <typename VertexT, typename GraphT>
  void discover_vertex(VertexT vertex, GraphT &graph) {
    for (auto visitor : m_visitors)
      discover_vertex_helper(visitor, vertex, graph);
  }

  template <typename VertexT, typename GraphT>
  void examine_vertex(VertexT vertex, GraphT &graph) {
    for (auto visitor : m_visitors)
      examine_vertex_helper(visitor, graph);
  }

  template <typename EdgeT, typename GraphT>
  void examine_edge(EdgeT edge, GraphT &graph) {
    for (auto visitor : m_visitors)
      examine_edge_helper(visitor, edge, graph);
  }

  template <typename EdgeT, typename GraphT>
  void tree_edge(EdgeT edge, GraphT &graph) {
    for (auto visitor : m_visitors)
      tree_edge_helper(visitor, edge, graph);
  }

  template <typename EdgeT, typename GraphT>
  void non_tree_edge(EdgeT edge, GraphT &graph) {
    for (auto visitor : m_visitors)
      non_tree_edge_helper(visitor, edge, graph);
  }

  template <typename EdgeT, typename GraphT>
  void gray_target(EdgeT edge, GraphT &graph) {
    for (auto visitor : m_visitors)
      color_target_helper(visitor, edge, graph, Color::GRAY);
  }

  template <typename EdgeT, typename GraphT>
  void black_target(EdgeT edge, GraphT &graph) {
    for (auto visitor : m_visitors)
      color_target_helper(visitor, edge, graph, Color::BLACK);
  }

  template <typename VertexT, typename GraphT>
  void finish_vertex(VertexT vertex, GraphT &graph) {
    for (auto visitor : m_visitors)
      finish_vertex_helper(visitor, vertex, graph);
  }
};

// TODO Implement defaults
template <
    VertexListGraphConcept GraphT, typename VisitorT = NullVisitor,
    typename ColorMapT, // decltype(make_two_bit_color_map(graph.num_vertices(),
                        // vertex_index_map ? vertex_index_map :
                        // const_pmap(graph, vertex_index))
    typename VertexIndexMapT, // decltype(const_pmap(graph, vertex_index))
    typename BufferT = std::queue<typename GraphT::vertex_descriptor>>
void bfs(const GraphT &const_graph, typename GraphT::vertex_descriptor source,
         VisitorT visitor, ColorMapT color, VertexIndexMapT vertex_index_map,
         BufferT buffer) {
  GraphT &graph = const_cast<GraphT &>(const_graph);
  bfs(graph, source, visitor, color, vertex_index_map, buffer);
}

template <IncidenceGraphConcept GraphT, typename VisitorT = NullVisitor,
          typename ColorMapT, // defaults to decltype(choose_pmap)(graph,
                              // vertex_color)
          typename VertexIndexMapT,
          typename BufferT = std::queue<typename GraphT::vertex_descriptor>>
void bfs_visit(const GraphT &const_graph,
               typename GraphT::vertex_descriptor source, VisitorT visitor,
               ColorMapT color_map, VertexIndexMapT vertex_index_map,
               BufferT buffer) {
  GraphT &graph = const_cast<GraphT &>(const_graph);
  bfs_visit(graph, source, buffer, visitor, color_map);
}

template <typename GraphT, typename SourceT> class BFSImplementation {
  using result_type = void;

  template <typename Args>
  void operator()(const GraphT &const_graph, const SourceT &const_source,
                  const Args &args) {
    typename GraphT::vertex_descriptor sources[1] = {const_source};
    std::queue<typename GraphT::vertex_descriptor> queue;
    bfs(const_graph, &sources[0], &sources[1], queue, NullVisitor(),
        make_color_map(const_graph));
  }
};

template <typename T> struct type_t { using type = T; };

template <GraphConcept GraphT, typename VertexDescriptorT,
          typename VisitorT = NullVisitor,
          ReadablePropertyMapConcept VertexIndexMapT = NullPropertyMap,
          ReadWritePropertyMapConcept ColorMapT = NullPropertyMap,
          typename BufferT = std::queue<typename GraphT::vertex_descriptor>>
requires VisitorConcept<VisitorT, GraphT> and
    ReadWritePropertyMapConcept<ColorMapT>
struct BFSParameters {
  type_t<GraphT> graph_t;
  type_t<VertexDescriptorT> vertex_descriptor_t;
  type_t<VisitorT> visitor_t;
  type_t<VertexIndexMapT> vertex_index_map_t;
  type_t<ColorMapT> color_map_t;
  type_t<BufferT> buffer_t;
};

#endif
