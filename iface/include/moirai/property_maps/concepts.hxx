#ifndef PROPERTY_MAPS_CONCEPTS
#define PROPERTY_MAPS_CONCEPTS

#include <concepts>
#include <cstddef>
#include <type_traits>

template <typename MapT>
concept PropertyMapConcept = requires() {
  typename MapT::key_type;
  typename MapT::value_type;
  typename MapT::reference;

  // std::same_as<typename MapT::key_type, KeyT>;
};

template <typename MapT>
concept ReadablePropertyMapConcept = PropertyMapConcept<MapT> and
    requires(const MapT &map, const typename MapT::key_type &key) {

  requires std::convertible_to<typename MapT::reference,
                               typename MapT::value_type>;
  { map.get(key) } -> std::same_as<typename MapT::value_type>;
};

template <typename MapT>
concept WritablePropertyMapConcept = PropertyMapConcept<MapT> and
    requires(MapT &map, const typename MapT::key_type &key,
             const typename MapT::value_type &value) {
  requires std::convertible_to<typename MapT::reference, void>;

  {map.put(key, value)};
};

template <typename MapT>
concept ReadWritePropertyMapConcept = PropertyMapConcept<MapT> and
    ReadablePropertyMapConcept<MapT> and WritablePropertyMapConcept<MapT>;

template <typename MapT>
concept LValuePropertyMapConcept =
    PropertyMapConcept<MapT> and ReadablePropertyMapConcept<MapT> and
    (std::same_as<const typename MapT::value_type &,
                  typename MapT::reference> or
     std::same_as<typename MapT::value_type &, typename MapT::reference>) and
    requires(const typename MapT::key_type &const_key) {
  { operator[](const_key) } -> std::same_as<const typename MapT::value_type &>;
};

template <typename MapT>
concept PropertyMapPtrConcept = LValuePropertyMapConcept<MapT> and
    std::same_as<typename MapT::reference, typename MapT::value_type &> and
    std::same_as<typename MapT::key_type, std::ptrdiff_t>;

template <typename MapT>
concept MutableLValuePropertyMapConcept = PropertyMapConcept<MapT> and
    ReadWritePropertyMapConcept<MapT> and
    std::same_as<typename MapT::value_type &, typename MapT::reference> and
    requires(const typename MapT::key_type &const_key) {
  { operator[](const_key) } -> std::same_as<typename MapT::value_type &>;
};
#endif
