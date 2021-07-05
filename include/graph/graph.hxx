#ifndef MOIRAI_GRAPH_GRAPH
#define MOIRAI_GRAPH_GRAPH
#include <ranges>
#include <vector>

template<template<typename...> class Container, class Property>
class PropertyMap
{
private:
  Container<Property> m_property_map;

public:
  typedef const Container<const Property> property_index_map;
  typedef typename Container<Property>::size_type property_descriptor;
};

template<class Vertex,
         class Edge,
         template<typename...> class VertexContainer = std::vector,
         template<typename...> class EdgeContainer = std::vector>
class Graph
{
private:
  typedef PropertyMap<VertexContainer, Vertex> VertexIndexPropertyMap;
  typedef PropertyMap<EdgeContainer, Edge> EdgeIndexPropertyMap;

  VertexIndexPropertyMap m_vertices;
  EdgeIndexPropertyMap m_edges;

  std::vector<size_t> out_edges;
  std::vector<size_t> in_edges;

public:
  typedef typename VertexIndexPropertyMap::property_index_map vertex_index_map;
  typedef typename EdgeIndexPropertyMap::property_index_map edge_index_map;

  typedef typename VertexContainer<Vertex>::size_type vertex_descriptor;
  typedef typename EdgeContainer<Edge>::size_type edge_descriptor;

  std::ranges::views::all_t<VertexContainer<Vertex>> vertices() const
  {
    return std::ranges::views::all(m_vertices.cbegin(), m_vertices.cend());
  }

  std::ranges::views::all_t<EdgeContainer<Edge>> edges() const
  {
    return std::ranges::views::all(m_edges.cbegin(), m_edges.cend());
  }

  const vertex_descriptor add_vetex(Vertex vertex)
  {
    m_vertices.push_back(vertex);
  }

  template<class... Args>
  const vertex_descriptor add_vetex(Args&&... args)
  {
    m_vertices.emplace_back(std::forward<Args>(args)...);
  }

  // vertex_index_map vertices() const;

  // edge_index_map edges() const;

  const vertex_descriptor source(const edge_descriptor&) const {
  }

  vertex_descriptor target(const edge_descriptor&) const;

  vertex_descriptor num_vertices() const;

  edge_descriptor num_edges() const;

  friend std::ranges::views::all_t<
    Graph<Vertex, Edge, VertexContainer, EdgeContainer>>
  get();
};
#endif
