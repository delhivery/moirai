#ifndef GRAPH_ALGORITHMS_DIJKSTRA_CONCEPTS
#define GRAPH_ALGORITHMS_DIJKSTRA_CONCEPTS

#include <moirai/graph/visitors/concepts.hxx>

template <typename VisitorT, typename GraphT>
concept DijkstraVisitorConcept =
    InitializesVertexVisitorConcept<VisitorT, GraphT> and
    DiscoversVertexVisitorConcept<VisitorT, GraphT> and
    NotRelaxedEdgeVisitorConcept<VisitorT, GraphT> and
    EdgeRelaxedVisitorConcept<VisitorT, GraphT> and
    ExaminesEdgeVisitorConcept<VisitorT, GraphT> and
    ExaminesVertexVisitorConcept<VisitorT, GraphT> and
    FinishesVertexVisitorConcept<VisitorT, GraphT>;
#endif
