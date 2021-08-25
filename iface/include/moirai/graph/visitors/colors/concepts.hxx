#ifndef GRAPH_VISITORS_COLORS_CONCEPTS
#define GRAPH_VISITORS_COLORS_CONCEPTS

#include <moirai/graph/visitors/colors.hxx>
#include <moirai/graph/visitors/concepts.hxx>

template <typename VisitorT, typename GraphT, Color ColorT>
concept ColorTargetVisitorConcept = VisitorConcept<VisitorT, GraphT> and
    requires(VisitorT visitor, GraphT &graph, Color color,
             typename GraphT::edge_descriptor edge) {
  { visitor.color_target(color, edge, graph) } -> std::same_as<void>;
};

#endif
