#ifndef MOIRAI_CONTAINERS_D_HEAP
#define MOIRAI_CONTAINERS_D_HEAP

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace ambasta {
namespace detail {
template<typename T, size_t Ratio>
class DHeap
{
  typedef typename std::ptrdiff_t difference_type;
  typedef T value_type;

  // input_iterator requirements
  //
  // default init
  // movable
  // copyable not necessary
  //
  // equality comparable w/ sentinel (end)
  //
  // must implement operator++ and operator++(int)
  // operator++ must return, operator++(int) can optionally return
  //
  // must implement
  // value_type operator*()
  //
  //
  // forward_iterator adds copy constructible as requirement
  //
  // begin and end can be member of container
  //               or be free functions in the same namespace
  //               or be hidden friend functions
  //
  // return type of begin and end doesn't need to be same.
  // end can return a sentinel which is equality comparable w/ an iterator
  //
  // for free/friend functions, both const and non-const versions are required
  // auto begin(const Container&); - const iterator
  // auto begin(Container&); - non const iterator
  //
  // immutable containers only need const iterators
};
}

template<typename T, typename C, size_t Ratio>
class DHeap : C<T, Ratio>
{
  static const bool is_mutable = C::is_mutable;

  typedef void* handle_type;

  DHeap(DHeap const& other)
    : C(other)
  {}

  DHeap(DHeap&& other)
    : C(std::move(other))
  {}

  DHeap& operator=(DHeap&& other)
  {
    C::operator=(std::move(other));
    return *this;
  }

  DHeap& operator=(DHeap const& other)
  {
    C::operator=(other);
    return *this;
  }

  bool empty(void) const { return C::empty(); }

  size_t size(void) const { return C::size(); }

  size_t max_size(void) const { return C::max_size(); }

  void clear(void) { C::clear(); }

  T const& top(void) const { return C::top(); }

  typename std::conditional_t<is_mutable, handle_type, void> push(T const& t)
  {
    return C::push(t);
  }

  template<class... Args>
  typename std::conditional_t<is_mutable, handle_type, void> emplace(
    Args&&... args)
  {
    return C::emplace(std::forward(args)...);
  }

  auto operator<=>(const DHeap&) const = default;

  std::enable_if_t<C::is_mutable, void> update(handle_type handle, const T& t)
  {
    C::update(handle, t);
  }

  std::enable_if_t<C::is_mutable, void> update(handle_type handle)
  {
    C::update(handle);
  }

  std::enable_if_t<C::is_mutable, void> increase(handle_type handle, const T& t)
  {
    C::increase(handle, t);
  }

  std::enable_if_t<C::is_mutable, void> increase(handle_type handle)
  {
    C::increase(handle);
  }

  std::enable_if_t<C::is_mutable, void> decrease(handle_type handle, const T& t)
  {
    C::decrease(handle, t);
  }

  std::enable_if_t<C::is_mutable, void> decrease(handle_type handle)
  {
    C::decrease(handle);
  }

  std::enable_if_t<C::is_mutable, void> erase(handle_type handle)
  {
    C::erase(handle);
  }

  static std::enable_if_t<C::is_mutable, handle_type> handle_from_iterator(
    auto const& iterator)
  {
    return C::handle_from_iterator(iterator);
  }

  void pop(void) { C::pop(); }

  void swap(DHeap& other) { C::swap(other); }

  const auto cbegin(void) { return C::cbegin(); }

  const auto cend(void) { return C::cend(); }

  auto begin(void) { return C::begin(); }

  auto end(void) { return C::end(); }
};
}
#endif
