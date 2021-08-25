#ifndef PROPERTY_MAPS_CONCEPTS
#define PROPERTY_MAPS_CONCEPTS

#include <concepts>
#include <cstddef>
#include <type_traits>

template <typename MapT, typename KeyT>
concept PropertyMapConcept = requires() {
  typename MapT::key_type;
  typename MapT::value_type;
  typename MapT::reference;

  std::same_as<typename MapT::key_type, KeyT>;
};

template <typename SequenceContainerT, typename ValueT>
requires std::is_convertible_v<SequenceContainerT, ValueT>
inline void put(SequenceContainerT *container, std::ptrdiff_t key,
                const ValueT &value) {
  container[key] = value;
}

template <typename SequenceContainerT>
inline const SequenceContainerT &get(const SequenceContainerT *container,
                                     std::ptrdiff_t key) {
  return container[key];
}

template <typename MapT, typename KeyT>
concept ReadablePropertyMapConcept = PropertyMapConcept<MapT, KeyT> and
    requires(const MapT &pmap, const typename MapT::key_type &key) {

  requires std::convertible_to<typename MapT::reference,
                               typename MapT::value_type>;
  { get(pmap, key) } -> std::same_as<typename MapT::value_type>;
};

template <typename MapT, typename KeyT>
concept WritablePropertyMapConcept = PropertyMapConcept<MapT, KeyT> and
    requires(MapT &map, const typename MapT::key_type &key,
             const typename MapT::value_type &value) {
  requires std::convertible_to<typename MapT::reference, void>;

  {put(map, key, value)};
};

template <typename MapT, typename KeyT>
concept ReadWritePropertyMapConcept = PropertyMapConcept<MapT, KeyT> and
    ReadablePropertyMapConcept<MapT, KeyT> and
    WritablePropertyMapConcept<MapT, KeyT>;

template <typename MapT, typename KeyT>
concept LValuePropertyMapConcept =
    PropertyMapConcept<MapT, KeyT> and
    ReadablePropertyMapConcept<MapT, KeyT> and
    (std::same_as<const typename MapT::value_type &,
                  typename MapT::reference> or
     std::same_as<typename MapT::value_type &, typename MapT::reference>) and
    requires(const typename MapT::key_type &const_key) {
  { operator[](const_key) } -> std::same_as<const typename MapT::value_type &>;
};

template <typename MapT, typename KeyT>
concept MutableLValuePropertyMapConcept = PropertyMapConcept<MapT, KeyT> and
    ReadWritePropertyMapConcept<MapT, KeyT> and
    std::same_as<typename MapT::value_type &, typename MapT::reference> and
    requires(const typename MapT::key_type &const_key) {
  { operator[](const_key) } -> std::same_as<typename MapT::value_type &>;
};
#endif
