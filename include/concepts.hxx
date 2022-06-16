#ifndef MOIRAI_CONCEPTS
#define MOIRAI_CONCEPTS

#include <concepts>
#include <ranges>

template<typename RangeT, typename ValueT>
concept range_of = std::ranges::range<RangeT> and
  std::same_as<std::decay_t<std::ranges::range_value_t<RangeT>>, ValueT>;

#endif
