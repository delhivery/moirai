#ifndef MOIRAI_ENUMERATE
#define MOIRAI_ENUMERATE

#include <tuple>

template<typename T,
         typename Iter = decltype(std::begin(std::declval<T>())),
         typename = decltype(std::end(std::declval<T>()))>
constexpr auto
enumerate(T&& iterable)
{
  struct iterator
  {
    std::size_t idx;
    Iter iter;

    auto operator!=(const iterator& other) const -> bool
    {
      return iter != other.iter;
    }

    void operator++()
    {
      ++idx;
      ++iter;
    }

    auto operator*() const { return std::tie(idx, iter); }
  };

  struct iterable_wrapper
  {
    T iterable;
    auto begin() { return iterator{ 0, std::begin(iterable) }; }
    auto end() { return iterator{ 0, std::end(iterable) }; }
  };

  return iterable_wrapper{ std::forward<T>(iterable) };
}

#endif
