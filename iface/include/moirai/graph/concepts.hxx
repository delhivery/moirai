#ifndef GRAPH_CONCEPTS
#define GRAPH_CONCEPTS

#include <concepts>
#include <iterator>

template <typename GraphT>
concept GraphConcept = std::is_default_constructible_v<GraphT> and
    std::is_copy_constructible_v<GraphT> and std::is_swappable_v<GraphT> and
    std::equality_comparable<GraphT> and requires() {
  typename GraphT::vertex_descriptor;
  typename GraphT::edge_descriptor;
};

template <typename GraphT>
concept IncidenceGraphConcept = GraphConcept<GraphT> and
    requires(GraphT graph, const GraphT const_graph,
             typename GraphT::vertex_descriptor vertex,
             typename GraphT::edge_descriptor edge) {
  typename GraphT::out_edge_iterator;
  typename GraphT::degree_size_type;

  requires not std::same_as<typename GraphT::out_edge_iterator, void>;
  requires not std::same_as<typename GraphT::degree_size_type, void>;
  requires std::forward_iterator<typename GraphT::out_edge_iterator>;
  requires std::is_default_constructible_v<typename GraphT::edge_descriptor>;
  requires std::equality_comparable<typename GraphT::edge_descriptor>;
  requires std::is_copy_constructible_v<typename GraphT::edge_descriptor>;
  requires std::is_swappable_v<typename GraphT::edge_descriptor>;

  requires std::same_as < std::iter_value_t<typename GraphT::out_edge_iterator>,
  typename GraphT::edge_descriptor > ;

  // TODO Add ranges support
  graph.out_edges(vertex);
  graph.out_degree(vertex);
  graph.source(edge);
  graph.target(edge);
  const_graph.out_edges(vertex);
  const_graph.out_degree(vertex);
  const_graph.source(edge);
  const_graph.target(edge);
  // TODONE Add ranges support
  {
    out_edges(vertex, graph)
    } -> std::same_as<std::pair<typename GraphT::out_edge_iterator,
                                typename GraphT::out_edge_iterator>>;

  {
    out_degree(vertex, graph)
    } -> std::same_as<typename GraphT::degree_size_type>;

  // { *iterators.first } -> std::same_as<typename GraphT::edge_descriptor>;

  { source(edge, graph) } -> std::same_as<typename GraphT::vertex_descriptor>;

  { target(edge, graph) } -> std::same_as<typename GraphT::vertex_descriptor>;

  {
    out_edges(vertex, const_graph)
    } -> std::same_as<std::pair<typename GraphT::out_edge_iterator,
                                typename GraphT::out_edge_iterator>>;
  {
    out_degree(vertex, const_graph)
    } -> std::same_as<typename GraphT::degree_size_type>;
  {
    source(edge, const_graph)
    } -> std::same_as<typename GraphT::vertex_descriptor>;
  {
    target(edge, const_graph)
    } -> std::same_as<typename GraphT::edge_descriptor>;
};

template <typename GraphT>
concept BidirectionGraphConcept = IncidenceGraphConcept<GraphT> and
    requires(GraphT graph, const GraphT const_graph,
             typename GraphT::vertex_descriptor vertex,
             typename GraphT::edge_descriptor edge) {
  typename GraphT::in_edge_iterator;
  requires std::forward_iterator<typename GraphT::in_edge_iterator>;
  requires not std::same_as<typename GraphT::in_edge_iterator, void>;
  requires std::same_as < std::iter_value_t<typename GraphT::in_edge_iterator>,
  typename GraphT::edge_descriptor > ;

  // TODO Ranges support
  graph.in_edges(vertex);
  graph.in_degree(vertex);
  graph.degree(vertex);
  const_graph.in_edges(vertex);
  const_graph.in_degree(vertex);
  const_graph.degree(vertex);
  // Ranges support

  {
    in_edges(vertex, graph)
    } -> std::same_as<std::pair<typename GraphT::in_edge_iterator,
                                typename GraphT::in_edge_iterator>>;
  {
    in_degree(vertex, graph)
    } -> std::same_as<typename GraphT::degree_size_type>;

  { degree(vertex, graph) } -> std::same_as<typename GraphT::degree_size_type>;

  {
    in_edges(vertex, const_graph)
    } -> std::same_as<std::pair<typename GraphT::in_edge_iterator,
                                typename GraphT::in_edge_iterator>>;

  {
    in_degree(vertex, const_graph)
    } -> std::same_as<typename GraphT::degree_size_type>;

  { degree(vertex, graph) } -> std::same_as<typename GraphT::degree_size_type>;
};

template <typename GraphT>
concept AdjacencyGraphConcept = GraphConcept<GraphT> and
    requires(GraphT graph, const GraphT const_graph,
             typename GraphT::vertex_descriptor vertex,
             typename GraphT::edge_descriptor edge) {
  typename GraphT::adjacency_iterator;
  requires std::forward_iterator<typename GraphT::adjacency_iterator>;
  requires not std::same_as<typename GraphT::adjacency_iterator, void>;
  requires std::same_as <
      std::iter_value_t<typename GraphT::adjacency_iterator>,
  typename GraphT::vertex_descriptor > ;

  // TODO Ranges support
  graph.adjacent_vertices(vertex);
  const_graph.adjacent_vertices(vertex);
  // Ranges support

  {
    adjacent_vertices(vertex, graph)
    } -> std::same_as<std::pair<typename GraphT::adjacency_iterator,
                                typename GraphT::adjacency_iterator>>;

  {
    adjacent_vertices(vertex, const_graph)
    } -> std::same_as<std::pair<typename GraphT::adjacency_iterator,
                                typename GraphT::adjacency_iterator>>;
};

template <typename GraphT>
concept VertexListGraphConcept = GraphConcept<GraphT> and
    requires(GraphT graph, const GraphT const_graph) {
  typename GraphT::vertex_iterator;
  typename GraphT::vertices_size_type;

  requires std::forward_iterator<typename GraphT::vertex_iterator>;
  requires not std::same_as<typename GraphT::vertex_iterator, void>;
  requires std::same_as < std::iter_value_t<typename GraphT::vertex_iterator>,
  typename GraphT::vertex_descriptor > ;

  requires not std::same_as<typename GraphT::vertices_size_type, void>;

  {
    graph.vertices()
    } -> std::same_as<std::pair<typename GraphT::vertex_iterator,
                                typename GraphT::vertex_iterator>>;

  {
    const_graph.vertices()
    } -> std::same_as<std::pair<typename GraphT::vertex_iterator,
                                typename GraphT::vertex_iterator>>;
  {
    const_graph.num_vertices()
    } -> std::same_as<typename GraphT::vertices_size_type>;
};

template <typename GraphT>
concept EdgeListGraphConcept = GraphConcept<GraphT> and
    requires(GraphT graph, const GraphT const_graph,
             typename GraphT::edge_descriptor edge) {
  typename GraphT::edge_iterator;
  typename GraphT::edges_size_type;

  std::forward_iterator<typename GraphT::edge_iterator>;
  std::is_default_constructible_v<typename GraphT::edge_descriptor>;
  std::equality_comparable<typename GraphT::edge_descriptor>;
  std::is_copy_constructible_v<typename GraphT::edge_descriptor>;
  std::is_swappable_v<typename GraphT::edge_descriptor>;

  not std::same_as<typename GraphT::edge_iterator, void>;
  not std::same_as<typename GraphT::edges_size_type, void>;

  std::same_as<std::iter_value_t<typename GraphT::edge_iterator>,
               typename GraphT::edge_descriptor>;

  {
    graph.edges()
    } -> std::same_as<std::pair<typename GraphT::edge_iterator,
                                typename GraphT::edge_iterator>>;

  { graph.source(edge) } -> std::same_as<typename GraphT::vertex_descriptor>;

  { graph.target(edge) } -> std::same_as<typename GraphT::vertex_descriptor>;

  {
    const_graph.edges()
    } -> std::same_as<std::pair<typename GraphT::edge_descriptor,
                                typename GraphT::edge_descriptor>>;

  {
    const_graph.source(edge)
    } -> std::same_as<typename GraphT::vertex_descriptor>;
  {
    const_graph.target(edge)
    } -> std::same_as<typename GraphT::vertex_descriptor>;
  { const_graph.num_edges() } -> std::same_as<typename GraphT::edges_size_type>;
};

template <typename GraphT>
concept VertexAndEdgeListGraphConcept =
    VertexListGraphConcept<GraphT> and EdgeListGraphConcept<GraphT>;
#endif
