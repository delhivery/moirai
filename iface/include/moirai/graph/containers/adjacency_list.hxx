#ifndef GRAPH_CONTAINERS_ADJACENCY_LIST
#define GRAPH_CONTAINERS_ADJACENCY_LIST
#include <functional>
#include <iterator>
#include <memory>
#include <moirai/graph/concepts.hxx>
#include <moirai/graph/containers/concepts.hxx>
#include <moirai/property.hxx>
#include <moirai/property/concepts.hxx>
#include <ranges>
#include <type_traits>

template <typename T>
concept DirectionConcept = requires() {
  typename T::is_directed;
  typename T::is_bidirectional;

  std::same_as<typename T::is_directed, bool>;
  std::same_as<typename T::is_bidirectional, bool>;
};

class Directed {
public:
  static constexpr bool is_directed = true;
  static constexpr bool is_bidirectional = false;
};

class Undirected {
public:
  static constexpr bool is_directed = false;
  static constexpr bool is_bidirectional = false;
};

class Bidirectional {
public:
  static constexpr bool is_directed = true;
  static constexpr bool is_bidirectional = true;
};

namespace concepts {
template <typename T>
concept LessThanComparable = std::strict_weak_order<std::less<T>, T, T>;
} // namespace concepts

template <typename VertexT>
requires std::equality_comparable<VertexT> and
    std::strict_weak_order<std::less<VertexT>, VertexT, VertexT>
class StoredEdge {
private:
  static NoProperty s_property;
  VertexT m_target;

public:
  inline StoredEdge() {}
  inline StoredEdge(VertexT target, const NoProperty & = NoProperty())
      : m_target(target) {}

  inline VertexT &target(void) const { return m_target; }

  inline const NoProperty &property() const { return s_property; }

  inline bool operator==(const StoredEdge &other) const {
    return m_target == other.m_target;
  }

  inline bool operator<(const StoredEdge &other) const {
    return m_target < other.m_target;
  }
};

template <typename VertexT, typename PropertyT>
requires StoredEdgeConcept<StoredEdge<VertexT>>
class StoredEdgeProperty : public StoredEdge<VertexT> {
  using property_type = PropertyT;
  using Base = StoredEdge<VertexT>;

  std::unique_ptr<PropertyT> m_property;

public:
  inline StoredEdgeProperty() {}

  inline StoredEdgeProperty(VertexT target,
                            const PropertyT &property = PropertyT())
      : Base(target), m_property(new PropertyT(property)) {}

  StoredEdgeProperty(StoredEdgeProperty &&other)
      : Base(static_cast<Base &&>(other)),
        m_property(std::move(other.m_property)) {}

  StoredEdgeProperty(StoredEdgeProperty const &other)
      : Base(static_cast<const Base &>(other)),
        m_property(
            std::move(const_cast<StoredEdgeProperty &>(other).m_property)) {}

  StoredEdgeProperty &operator=(StoredEdgeProperty &&other) {
    Base::operator=(static_cast<const Base &>(other));
    m_property = std::move(const_cast<StoredEdgeProperty &>(other).m_property);
    return *this;
  }

  inline PropertyT &property() { return *m_property; }

  inline const PropertyT &property() const { return *m_property; }
};

template <typename VertexT, typename IteratorT, typename PropertyT>
requires std::input_iterator<IteratorT> and
    StoredEdgeConcept<StoredEdge<VertexT>>
class StoredEdgeIterator : public StoredEdge<VertexT> {
  using property_type = PropertyT;
  using Base = StoredEdge<VertexT>;

protected:
  IteratorT m_iterator;

public:
  inline StoredEdgeIterator() {}

  inline StoredEdgeIterator(VertexT vertex) : Base(vertex) {}

  inline StoredEdgeIterator(VertexT vertex, IteratorT iterator, void * = 0)
      : Base(vertex), m_iterator(iterator) {}

  inline PropertyT &property() { return m_iterator->property(); }

  inline const PropertyT &property() const { return m_iterator->property(); }

  inline IteratorT iterator() const { return m_iterator; }
};

template <typename VertexT, typename EdgeContainerT, typename PropertyT>
requires std::ranges::random_access_range<EdgeContainerT>
class StoredRandomAccessEdgeIterator : public StoredEdge<VertexT> {
  using property_type = PropertyT;
  using Base = StoredEdge<VertexT>;
  using IteratorT = std::ranges::iterator_t<EdgeContainerT>;

protected:
  std::size_t m_index;
  EdgeContainerT *m_edge_container;

public:
  inline StoredRandomAccessEdgeIterator() {}

  inline explicit StoredRandomAccessEdgeIterator(VertexT vertex)
      : Base(vertex), m_index(0), m_edge_container(0) {}

  inline StoredRandomAccessEdgeIterator(VertexT vertex, IteratorT iterator,
                                        EdgeContainerT *edge_container)
      : Base(vertex), m_index(iterator - edge_container->begin()),
        m_edge_container(edge_container) {}

  inline PropertyT &property() {
    static_assert(m_edge_container != 0);
    return (*m_edge_container)[m_index].property();
  }

  inline const PropertyT &property() const {
    static_assert(m_edge_container != 0);
    return (*m_edge_container)[m_index].property();
  }

  inline IteratorT iterator() const {
    static_assert(m_edge_container != 0);
    return m_edge_container->begin() + m_index;
  }
};
#endif
