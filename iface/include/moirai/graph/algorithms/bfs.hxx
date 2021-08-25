#ifndef GRAPH_ALGORITHMS_BFS
#define GRAPH_ALGORITHMS_BFS
#include <moirai/graph/concepts.hxx>
#include <moirai/graph/visitors.hxx>
#include <moirai/graph/visitors/concepts.hxx>
#include <moirai/property_maps/concepts.hxx>
#include <ranges>

template <IncidenceGraphConcept GraphT, typename BufferT, typename VisitorT,
          typename ColorMapT, typename SourceIteratorT>
requires VisitorConcept<VisitorT, GraphT> and
    ReadWritePropertyMapConcept<ColorMapT, typename GraphT::vertex_descriptor>
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
void bfs_search(const GraphT &graph, SourceIteratorT source,
                SourceIteratorT target, BufferT &queue, VisitorT visitor,
                ColorMapT color_map) {
  using Color = typename ColorMapT::value_type;

  for (auto vertex : graph.vertices()) {
    visitor.initialize_vertex(*vertex, graph);
    put(color_map, *vertex, Color::white());
  }
  bfs_visit(graph, source, target, queue, visitor, color_map);
}

template <VertexListGraphConcept GraphT, typename BufferT, typename VisitorT,
          typename ColorMapT>
void bfs_search(const GraphT &graph, typename GraphT::vertex_descriptor source,
                BufferT &queue, VisitorT visitor, ColorMapT color_map) {
  typename GraphT::vertex_descriptor sources[1] = {source};
  bfs_search(graph, sources, sources + 1, queue, visitor, color_map);
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

template <VertexListGraphConcept GraphT, typename P, typename T, typename R>
void bfs(const GraphT &graph, typename GraphT::vertex_descriptor source) {
  GraphT &const_graph = const_cast<GraphT &>(graph);
  // TODO Move from tag dispatch to constraints
  bfs_dispatch(const_graph, source);
}
#endif
