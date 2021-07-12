#ifndef MOIRAI_GRAPH
#define MOIRAI_GRAPH

#include <array>
#include <iterator>
#include <map>
#include <memory>
// #include <ranges>
#include <limits>
#include <list>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "graph/direction.hxx"
#include "graph/edge.hxx"

class no_property
{};

template<template<typename...> class Selector, class ValueTypeT>
class ContainerGenerator
{
  using type = Selector<ValueTypeT>;
};

template<class ValueTypeT>
class ContainerGenerator<std::map, ValueTypeT>
{
  using type = std::set<ValueTypeT>;
};

template<class ValueTypeT>
class ContainerGenerator<std::multimap, ValueTypeT>
{
  using type = std::multiset<ValueTypeT>;
};

template<class ValueTypeT>
class ContainerGenerator<std::unordered_map, ValueTypeT>
{
  using type = std::unordered_set<ValueTypeT>;
};

template<template<typename...> class StorageT, class ValueTypeT>
class supports_parallel_edges : std::true_type
{};

// Disallow for set, hashSet, map, hashmap,
template<class ValueTypeT>
class supports_parallel_edges<std::set, ValueTypeT> : std::false_type
{};

template<class ValueTypeT>
class supports_parallel_edges<std::map, ValueTypeT> : std::false_type
{};

template<class ValueTypeT>
class supports_parallel_edges<std::unordered_map, ValueTypeT> : std::false_type
{};

namespace detail {
template <class Base> class AdjacencyListHelper: public Base {
    auto adjacent_vertices(
};

template<class Derived>
class AdjacencyListImplementation
{
  typedef typename std::vector<size_t>::iterator::iterator_category type;
};

template<typename Container>
using IteratorCategoryOf = typename std::iterator_traits<
  typename Container::iterator>::iterator_category;

template<typename T, typename V>
constexpr bool on_edge_storage = std::conditional_t<T::value and not V::value,
                                                    std::true_type,
                                                    std::false_type>::value;

template<template<typename...> class OutEdgeListT = std::vector,
         template<typename...> class VertexListT = std::vector,
         Direction direction = Direction::Directed,
         template<typename...> class EdgeListT = std::list>
class AdjacencyListTraits
{
public:
  // using is_random_access = typename VertexListT::iterator::iterator_category
  // == std::random_access using is_bidirectional = typename
  // Direction::is_bidirectional_t
  //
  // using is_directed = typename Direcition::is_directional_t
  //
  // typedef std::conditional<bool, T, F>::type type
  // typedef mpl_if_<c, t1, t2>::type t;
  // means
  // t =  c::value? t1 : t2
  //
  //                    t0 = is_directed ? directed_tag : undirected_tag;
  // directed_category = is_bidirectional ? bidirectional_tag : t0
  //
  // using edge_parallel_category = parallel_edge_traits<OutEdgeListT>::type;
  typedef std::size_t VertexSizeT;
  typedef void* vertexPointerT;

  // VertexDescriptorT = is_rand_access ? VertexSizeT : vertexPointerT

  // using EdgeDescriptorT =
  // detail::EdgeDescriptorImplementation<directed_category, VertexDescriptorT>;
private:
  using EdgeContainerT =
    typename ContainerGenerator<EdgeListT, no_property>::type;
  using BidirectionalT = typename is_bidirectional_t<direction>::value;
  using DirectedT = typename is_directed<direction>::value;

  using EdgeSizeT =
    typename std::conditional<on_edge_storage<DirectedT, BidirectionalT>,
                              std::size_t,
                              typename EdgeContainerT::size_type>::type;
  // using OnEdgeStorageT =BidirectionalT and DirectedT;
};

template<class GraphT,
         template<typename...>
         class VertexListT,
         template<typename...>
         class OutEdgeListT,
         Direction direction,
         class VertexProperty,
         class EdgeProperty,
         template<typename...>
         class EdgeListT>
class AdjacencyListGenerator
{
  typedef typename AdjacencyListImplementation<GraphT>::type type;
};
}

template<template<typename...> class OutEdgeListT = std::vector,
         template<typename...> class VertexListT = std::vector,
         Direction direction = Direction::Directed,
         class VertexPropertyT = no_property,
         class EdgePropertyT = no_property,
         template<typename...> class EdgeListT = std::list>
class AdjacencyList
  : public detail::AdjacencyListGenerator<AdjacencyList<OutEdgeListT,
                                                        VertexListT,
                                                        direction,
                                                        VertexPropertyT,
                                                        EdgePropertyT,
                                                        EdgeListT>,
                                          VertexListT,
                                          OutEdgeListT,
                                          direction,
                                          VertexPropertyT,
                                          EdgePropertyT,
                                          EdgeListT>::type
{
private:
  using Base = typename detail::AdjacencyListGenerator<AdjacencyList,
                                                       VertexListT,
                                                       OutEdgeListT,
                                                       direction,
                                                       VertexPropertyT,
                                                       EdgePropertyT,
                                                       EdgeListT>::type;

public:
  AdjacencyList(const AdjacencyList& other)
    : Base(other)
  {}

  AdjacencyList& operator=(const AdjacencyList& other)
  {
    if (&other != this) {
      Base::operator=(other);
    }
    return *this;
  }

  AdjacencyList(VertexSizeT num_vertices)
    : Base(num_vertices)
  {}
};

template<class VertexPropertyT, class EdgePropertyT>
class Graph
{
private:
  using VertexDescriptorT = size_t;
  using EdgeDescriptorT = size_t;

  using NeighborsT =
    std::unordered_map<VertexDescriptorT, std::vector<EdgeDescriptorT>>;

  std::unordered_map<VertexDescriptorT, NeighborsT> m_out_nodes, m_inc_nodes;
  std::unordered_map<EdgeDescriptorT,
                     std::pair<VertexDescriptorT, VertexDescriptorT>>
    m_edge_vertices;

  std::vector<std::shared_ptr<VertexPropertyT>> m_vertices;
  std::vector<std::shared_ptr<EdgePropertyT>> m_edges;

public:
  Graph() = default;

  static VertexDescriptorT null_vertex()
  {
    return std::numeric_limits<VertexDescriptorT>::max();
  }

  static EdgeDescriptorT null_edge()
  {
    return std::numeric_limits<EdgeDescriptorT>::max();
  }

  template<class... Args>
  void add_vertex(const Args&&... args)
  {
    m_vertices.emplace_back(
      std::make_shared<VertexPropertyT>(std::forward<Args>(args)...));
  }

  template<class... Args>
  void add_edge(VertexDescriptorT source,
                VertexDescriptorT target,
                const Args&&... args)
  {
    m_edges.emplace_back(
      std::make_shared<EdgePropertyT>(std::forward<Args>(args)...));

    auto edge_descriptor = m_edges.size();

    if (not m_out_nodes.contains(source))
      m_out_nodes[source] = {};

    if (not m_inc_nodes.contains(target))
      m_inc_nodes[target] = {};

    m_out_nodes[source][target] = edge_descriptor;
    m_inc_nodes[target][source] = edge_descriptor;
    m_edge_vertices[edge_descriptor] = std::make_pair(source, target);
  }

  VertexDescriptorT source(VertexDescriptorT edge_descriptor)
  {
    if (m_edge_vertices.contains(edge_descriptor))
      return std::get<0>(m_edge_vertices[edge_descriptor]);
    // VertexDescriptor should be nullable
    return null_vertex();
  }

  VertexDescriptorT target(VertexDescriptorT edge_descriptor)
  {
    if (m_edge_vertices.contains(edge_descriptor))
      return std::get<1>(m_edge_vertices[edge_descriptor]);
    return null_vertex();
  }

  VertexDescriptorT num_vertices() {}
};
#endif
