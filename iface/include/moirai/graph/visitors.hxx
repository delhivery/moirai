#ifndef GRAPH_VISITORS
#define GRAPH_VISITORS

#include <moirai/graph/visitors/colors.hxx>
#include <moirai/graph/visitors/colors/concepts.hxx>
#include <moirai/graph/visitors/concepts.hxx>

template <typename VisitorT> class BaseVisitor {
  template <typename T, typename GraphT> void operator()(T, GraphT &) {}
};

class NullVisitor : BaseVisitor<NullVisitor> {
  template <typename T, typename GraphT> void no_event(T, GraphT &) {}
};

class NullVisitorList {
  // TODO Should be iterable, have 0 size
};

template <typename VisitorT, typename T, typename GraphT>
requires(VisitorConcept<VisitorT, GraphT>) void discover_vertex_helper(
    VisitorT, T, GraphT &) {}

template <typename VisitorT, typename T, typename GraphT>
requires(VisitorConcept<VisitorT, GraphT>) void examine_vertex_helper(
    VisitorT, T, GraphT &) {}

template <typename VisitorT, typename T, typename GraphT>
requires(VisitorConcept<VisitorT, GraphT>) void finish_vertex_helper(VisitorT,
                                                                     T,
                                                                     GraphT &) {
}

template <typename VisitorT, typename T, typename GraphT>
requires(VisitorConcept<VisitorT, GraphT>) void examine_edge_helper(VisitorT, T,
                                                                    GraphT &) {}

template <typename VisitorT, typename T, typename GraphT>
requires(VisitorConcept<VisitorT, GraphT>) void tree_edge_helper(VisitorT, T,
                                                                 GraphT &) {}

template <typename VisitorT, typename T, typename GraphT>
requires(VisitorConcept<VisitorT, GraphT>) void non_tree_edge_helper(VisitorT,
                                                                     T,
                                                                     GraphT &) {
}

template <typename VisitorT, typename T, typename GraphT, Color ColorT>
requires(VisitorConcept<VisitorT, GraphT>) void color_target_helper(VisitorT, T,
                                                                    GraphT &,
                                                                    Color) {}

template <typename VisitorT, typename T, typename GraphT>
requires(DiscoversVertexVisitorConcept<
         VisitorT, GraphT>) void discover_vertex_helper(VisitorT visitor,
                                                        T vertex,
                                                        GraphT &graph) {
  visitor.discover_vertex(vertex, graph);
}

template <typename VisitorT, typename T, typename GraphT>
requires(ExaminesVertexVisitorConcept<
         VisitorT, GraphT>) void examine_vertex_helper(VisitorT visitor,
                                                       T vertex,
                                                       GraphT &graph) {
  visitor.examine_vertex(vertex, graph);
}

template <typename VisitorT, typename T, typename GraphT>
requires(FinishesVertexVisitorConcept<
         VisitorT, GraphT>) void finish_vertex_helper(VisitorT visitor,
                                                      T vertex, GraphT &graph) {
  visitor.finish_vertex(vertex, graph);
}

template <typename VisitorT, typename T, typename GraphT>
requires(ExaminesEdgeVisitorConcept<VisitorT, GraphT>) void examine_edge_helper(
    VisitorT visitor, T edge, GraphT &graph) {
  visitor.examine_edge(edge, graph);
}

template <typename VisitorT, typename T, typename GraphT>
requires(TreeEdgeVisitorConcept<VisitorT, GraphT>) void tree_edge_helper(
    VisitorT visitor, T edge, GraphT &graph) {
  visitor.tree_edge(edge, graph);
}

template <typename VisitorT, typename T, typename GraphT>
requires(NonTreeEdgeVisitorConcept<VisitorT, GraphT>) void non_tree_edge_helper(
    VisitorT visitor, T edge, GraphT &graph) {
  visitor.non_tree_edge(edge, graph);
}

template <typename VisitorT, typename T, typename GraphT, Color ColorT>
requires(ColorTargetVisitorConcept<
         VisitorT, GraphT, ColorT>) void color_target_helper(VisitorT visitor,
                                                             T edge,
                                                             GraphT &graph,
                                                             Color color) {
  visitor.color_target(edge, graph, color);
}

#endif
