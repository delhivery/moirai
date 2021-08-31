#ifndef PROPERTY_MAPS
#define PROPERTY_MAPS

#include <cassert>
#include <iterator>
#include <moirai/graph/concepts.hxx>
#include <moirai/property_maps/concepts.hxx>

template <GraphConcept GraphT> class NullPropertyMap {
public:
  using key_type = void;
  using value_type = void *;
  using reference = void;

  void *get(void) {}

  void put(GraphT &, void *) {}
};

template <std::random_access_iterator IteratorT, typename IndexMapT,
          typename T = typename std::iterator_traits<IteratorT>::value_type,
          typename ReferenceT =
              typename std::iterator_traits<IteratorT>::reference>
class IteratorPropertyMap {
protected:
  IteratorT iterator;
  IndexMapT index_map;

public:
  using key_type = typename IndexMapT::key_type;
  using value_type = T;
  using reference = ReferenceT;
  inline IteratorPropertyMap(IteratorT iterator = IteratorT(),
                             const IndexMapT &index_map = IndexMapT())
      : iterator(iterator), index_map(index_map) {}

  inline ReferenceT operator[](key_type key) const {
    return *(iterator + get(index_map, key));
  }
};

template <std::random_access_iterator RAIterT, typename IndexMapT,
          typename T = typename std::iterator_traits<RAIterT>::value_type,
          typename ReferenceT =
              typename std::iterator_traits<RAIterT>::reference>
class SafeIteratorPropertyMap {
protected:
  RAIterT iterator;
  IndexMapT index_map;
  typename IndexMapT::value_type size;

public:
  using key_type = typename IndexMapT::key_type;
  using value_type = T;
  using reference = ReferenceT;

  inline SafeIteratorPropertyMap(RAIterT iterator, std::size_t size = 0,
                                 const IndexMapT &index_map = IndexMapT())
      : iterator(iterator), size(size), index_map(index_map) {}

  inline SafeIteratorPropertyMap() {}

  inline ReferenceT operator[](key_type key) const {
    assert(get(index_map, key) < size);
    return *(iterator + get(index_map, key));
  }
};

template <typename AssociativeContainerT> class AssociativePropertyMap {
private:
  AssociativeContainerT *m_container;

public:
  using key_type = typename AssociativeContainerT::key_type;
  using value_type = typename AssociativeContainerT::value_type;
  using reference = value_type &;

  AssociativePropertyMap() : m_container(0) {}

  AssociativePropertyMap(AssociativeContainerT &container)
      : m_container(container) {}

  reference operator[](const key_type &key) const {
    return (*m_container)[key];
  }
};

template <typename AssociativeContainerT> class ConstAssociativePropertyMap {
private:
  AssociativeContainerT *m_container;

public:
  using key_type = typename AssociativeContainerT::key_type;
  using value_type = typename AssociativeContainerT::value_type::second_type;
  using reference = const value_type &;

  ConstAssociativePropertyMap() : m_container(0) {}

  ConstAssociativePropertyMap(const AssociativeContainerT &container)
      : m_container(container) {}

  reference operator[](const key_type &key) const {
    return m_container->find(key)->second;
  }
};

template <typename ValueT, typename KeyT = void> class StaticPropertyMap {
private:
  ValueT m_value;

public:
  using key_type = KeyT;
  using value_type = ValueT;
  using reference = ValueT;
  StaticPropertyMap(ValueT value) : m_value(value) {}

  template <typename T> inline reference operator[](T) const { return m_value; }
};

template <typename KeyT, typename ValueT> class ReferencePropertyMap {
private:
  ValueT *m_value;

public:
  using key_type = KeyT;
  using value_type = ValueT;
  using reference = ValueT &;
  ReferencePropertyMap(ValueT &value) : m_value(value) {}

  ValueT &operator[](const KeyT &) const { return *m_value; }
};

template <typename T> class TypedIdentityPropertyMap {
public:
  using key_type = T;
  using value_type = T;
  using reference = T;

  inline value_type operator[](const key_type &key) const { return key; }
};

namespace detail {
class DummyPropertyMapReference {
public:
  template <typename T> DummyPropertyMapReference &operator=(const T &) {
    return *this;
  }

  operator int() { return 0; }
};
}; // namespace detail

class DummyPropertyMap {
public:
  using key_type = void;
  using value_type = int;
  using reference = detail::DummyPropertyMapReference;

protected:
  value_type m_value;

public:
  inline DummyPropertyMap() : m_value(0) {}

  inline DummyPropertyMap(value_type value) : m_value(value) {}

  inline DummyPropertyMap(const DummyPropertyMap &other)
      : m_value(other.m_value) {}

  template <typename T> inline reference operator[](T) const {
    return reference();
  }
};

template <typename PropertyMapT> class PropertyMapFunction {
  PropertyMapT m_map;
  using param_type = typename PropertyMapT::key_type;

public:
  explicit PropertyMapFunction(const PropertyMapT &map) : m_map(map) {}

  using result_type = typename PropertyMapT::value_type;

  result_type operator()(const param_type &key) const {
    return get(m_map, key);
  }
};

#endif
