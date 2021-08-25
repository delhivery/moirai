#ifndef GRAPH_ALGORITHMS_BFS_CONCEPTS
#define GRAPH_ALGORITHMS_BFS_CONCEPTS
#include <moirai/graph/visitors/colors/concepts.hxx>
#include <moirai/graph/visitors/concepts.hxx>

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

#endif
