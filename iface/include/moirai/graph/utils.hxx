#ifndef GRAPH_UTILS
#define GRAPH_UTILS
template <typename T> class TypeHelper { using type = T; };

template <typename T> inline constexpr TypeHelper<T> type{};

template <typename T, typename P> auto named_param_t() {
  return decltype(T::P)::type;
}

template <typename T, typename V> inline constexpr
#endif
