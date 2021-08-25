#ifndef GRAPH_VISITORS_CONCEPTS
#define GRAPH_VISITORS_CONCEPTS
#include <moirai/graph/concepts.hxx>

template <typename VisitorT, typename GraphT>
concept VisitorConcept =
    GraphConcept<GraphT> and std::is_copy_constructible_v<VisitorT>;

template <typename VisitorT, typename GraphT>
concept BaseVisitorConcept = VisitorConcept<VisitorT, GraphT> and
    requires(VisitorT visitor, GraphT &graph) {
  { visitor.no_event(graph) } -> std::same_as<void>;
};

template <typename VisitorT, typename GraphT>
concept InitializesVertexVisitorConcept = VisitorConcept<VisitorT, GraphT> and
    requires(VisitorT visitor, GraphT &graph,
             typename GraphT::vertex_descriptor vertex) {
  { visitor.initialize_vertex(vertex, graph) } -> std::same_as<void>;
};

template <typename VisitorT, typename GraphT>
concept DiscoversVertexVisitorConcept = VisitorConcept<VisitorT, GraphT> and
    requires(VisitorT visitor, GraphT &graph,
             typename GraphT::vertex_descriptor vertex) {
  { visitor.initialize_vertex(vertex, graph) } -> std::same_as<void>;
};

template <typename VisitorT, typename GraphT>
concept NotRelaxedEdgeVisitorConcept = VisitorConcept<VisitorT, GraphT> and
    requires(VisitorT visitor, GraphT &graph,
             typename GraphT::edge_descriptor edge) {
  { visitor.edge_not_relaxed(edge, graph) } -> std::same_as<void>;
};
template <typename VisitorT, typename GraphT>
concept EdgeRelaxedVisitorConcept = VisitorConcept<VisitorT, GraphT> and
    requires(VisitorT visitor, GraphT &graph,
             typename GraphT::edge_descriptor edge) {
  { visitor.edge_relaxed(edge, graph) } -> std::same_as<void>;
};

template <typename VisitorT, typename GraphT>
concept ExaminesEdgeVisitorConcept = VisitorConcept<VisitorT, GraphT> and
    requires(VisitorT visitor, GraphT &graph,
             typename GraphT::edge_descriptor edge) {
  { visitor.examine_edge(edge, graph) } -> std::same_as<void>;
};

template <typename VisitorT, typename GraphT>
concept ExaminesVertexVisitorConcept = VisitorConcept<VisitorT, GraphT> and
    requires(VisitorT visitor, GraphT &graph,
             typename GraphT::vertex_descriptor vertex) {
  { visitor.examine_vertex(vertex, graph) } -> std::same_as<void>;
};

template <typename VisitorT, typename GraphT>
concept FinishesVertexVisitorConcept = VisitorConcept<VisitorT, GraphT> and
    requires(VisitorT visitor, GraphT &graph,
             typename GraphT::vertex_descriptor vertex) {
  { visitor.finish_vertex(vertex, graph) } -> std::same_as<void>;
};

template <typename VisitorT, typename GraphT>
concept NonTreeEdgeVisitorConcept = VisitorConcept<VisitorT, GraphT> and
    requires(VisitorT visitor, GraphT &graph,
             typename GraphT::edge_descriptor edge) {
  { visitor.non_tree_edge(edge, graph) } -> std::same_as<void>;
};

template <typename VisitorT, typename GraphT>
concept TreeEdgeVisitorConcept = VisitorConcept<VisitorT, GraphT> and
    requires(VisitorT visitor, GraphT &graph,
             typename GraphT::edge_descriptor edge) {
  { visitor.tree_edge(edge, graph) } -> std::same_as<void>;
};

enum class Color : char { WHITE = 0, GRAY = 1, GREEN = 2, BLACK = 3 };

template <typename VisitorT, typename GraphT, Color ColorT>
concept ColorTargetVisitorConcept = VisitorConcept<VisitorT, GraphT> and
    requires(VisitorT visitor, GraphT &graph, Color color,
             typename GraphT::edge_descriptor edge) {
  { visitor.color_target(color, edge, graph) } -> std::same_as<void>;
};

template <typename VisitorT, typename GraphT>
concept DijkstraVisitorConcept =
    InitializesVertexVisitorConcept<VisitorT, GraphT> and
    DiscoversVertexVisitorConcept<VisitorT, GraphT> and
    NotRelaxedEdgeVisitorConcept<VisitorT, GraphT> and
    EdgeRelaxedVisitorConcept<VisitorT, GraphT> and
    ExaminesEdgeVisitorConcept<VisitorT, GraphT> and
    ExaminesVertexVisitorConcept<VisitorT, GraphT> and
    FinishesVertexVisitorConcept<VisitorT, GraphT>;

template <typename VisitorT, typename GraphT>
concept BFSVisitorConcept =
    InitializesVertexVisitorConcept<VisitorT, GraphT> and
    DiscoversVertexVisitorConcept<VisitorT, GraphT> and
    ExaminesEdgeVisitorConcept<VisitorT, GraphT> and
    ExaminesVertexVisitorConcept<VisitorT, GraphT> and
    FinishesVertexVisitorConcept<VisitorT, GraphT> and
    NonTreeEdgeVisitorConcept<VisitorT, GraphT> and
    TreeEdgeVisitorConcept<VisitorT, GraphT> and
    ColorTargetVisitorConcept<VisitorT, GraphT, Color::GRAY> and
    ColorTargetVisitorConcept<VisitorT, GraphT, Color::BLACK> and
    FinishesVertexVisitorConcept<VisitorT, GraphT>;

enum class VisitorEvents {
  no_event,
  initialize_vertex,
  start_vertex,
  discover_vertex,
  finish_vertex,
  examine_vertex,
  examine_edge,
  tree_edge,
  non_tree_edge,
  gray_target,
  black_target,
  forward_or_cross_edge,
  back_edge,
  finish_edge,
  edge_relaxed,
  edge_not_relaxed,
  edge_minimized,
  edge_not_minimized
};

#endif
