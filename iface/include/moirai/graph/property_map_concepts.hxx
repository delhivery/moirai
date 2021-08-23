#include <concepts>
#include <cstddef>
#include <type_traits>

template <typename MapT>
concept PropertyMapConcept = requires() {
  typename MapT::key_type;
  typename MapT::value_type;
  typename MapT::reference;
  // typename T::category;
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

template <typename MapT>
concept ReadablePropertyMapConcept = PropertyMapConcept<MapT> and
    requires(const MapT &pmap, const typename MapT::key_type &key) {

  requires std::convertible_to<typename MapT::reference,
                               typename MapT::value_type>;
  { get(pmap, key) } -> std::same_as<typename MapT::value_type>;
};

template <typename MapT>
concept WritablePropertyMapConcept = PropertyMapConcept<MapT> and
    requires(MapT &map, const typename MapT::key_type &key,
             const typename MapT::value_type &value) {
  requires std::convertible_to<typename MapT::reference, void>;

  {put(map, key, value)};
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
concept MutableLValuePropertyMapConcept = PropertyMapConcept<MapT> and
    ReadWritePropertyMapConcept<MapT> and
    std::same_as<typename MapT::value_type &, typename MapT::reference> and
    requires(const typename MapT::key_type &const_key) {
  { operator[](const_key) } -> std::same_as<typename MapT::value_type &>;
};
