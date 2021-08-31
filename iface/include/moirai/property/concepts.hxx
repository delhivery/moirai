#ifndef MOIRAI_PROPERTY_CONCEPTS
#define MOIRAI_PROPERTY_CONCEPTS
#include <concepts>

template <typename T>
concept PropertyConcept = requires() {
  typename T::next_type;
  typename T::value_type;
};

template <typename T>
concept TypedPropertyConcept = PropertyConcept<T> and requires() {
  typename T::type;
};

#endif
