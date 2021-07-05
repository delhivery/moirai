#ifndef MOIRAI_GRAPH_PROPERTY_MAP
#define MOIRAI_GRAPH_PROPERTY_MAP

#include <memory>
#include <vector>

namespace moirai {
namespace detail {

template<typename Container>
using IteratorCategoryOf = typename std::iterator_traits<
  typename Container::iterator>::iterator_category;

template<template<typename... Args> class ContainerT, class ValueType>
using container_gen = ContainerT<ValueType>;

template<class OutEdgeListT,
         class VertexListT,
         class DirectedT,
         template<typename... Args>
         class EdgeListT>
struct adjacency_list_traits
{
private:
  typedef container_gen<EdgeListT, void> EdgeContainer;
};
template<class OutEdgeListT,
         class VertexListT,
         class DirectedT,
         class VertexProperty,
         class EdgeProperty,
         class GraphProperty,
         class EdgeListT>
class AdjacencyListGenerator
{};
}

template<class OutEdgeListT,
         class VertexListT,
         class DirectedT,
         class VertexPropertyT,
         class EdgePropertyT,
         class GraphPropertyT,
         class EdgeListT>
class AdjacencyList
{
private:
  const std::unique_ptr<GraphPropertyT> m_property;

  using Base = detail::IteratorCategoryOf<VertexListT>;

public:
  AdjacencyList(const GraphPropertyT& graph_property = GraphPropertyT())
    : m_property(new GraphPropertyT(graph_property))
  {}

  AdjacencyList(const AdjacencyList& other)
    : Base(other)
    , m_property(new GraphPropertyT(*other.m_property))
  {}

  AdjacencyList& operator=(const AdjacencyList& other)
  {
    if (&other != this) {
      Base::operator=(other);

      std::unique_ptr<GraphPropertyT> property(
        new GraphPropertyT(*other.m_property));
      m_property.swap(property);
    }
    return *this;
  }

  AdjacencyList(size_t num_vertices,
                const GraphPropertyT& graph_property = GraphPropertyT())
    : Base(num_vertices)
    , m_property(new GraphPropertyT(graph_property))
  {}
};
}
#endif
