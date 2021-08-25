#ifndef GRAPH_VISITORS_VISITORS
#define GRAPH_VISITORS_VISITORS

#include <utility>

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

template <VisitorEvents EventT, typename VisitorT>
class VisitorFunctor : VisitorT {
  VisitorFunctor(const VisitorT &visitor) : VisitorT(visitor) {}
};

template <typename VisitorT, typename T, typename GraphT>
inline void invoke_dispatch(VisitorT &visitor, T t, GraphT &graph) {
  visitor(t, graph);
}

template <typename VisitorT, typename RestT, typename T, typename GraphT>
inline void invoke_visitors(std::pair<VisitorT, RestT> &visitors, T t,
                            GraphT &graph) {
  using Category = typename VisitorT::event_filter;
  // TODO Only invoke if visitor::event_filter
  // visitors.first(t, graph);
  invoke_dispatch(visitors.first, t, graph);
  invoke_visitors(visitors.second, t, graph);
}

template <typename VisitorT, typename T, typename GraphT>
inline void invoke_visitors(VisitorT &visitor, T t, GraphT &graph) {
  using Category = typename VisitorT::event_filter;
  invoke_dispatch(visitor, t, graph);
}

#endif
