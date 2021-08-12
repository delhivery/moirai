#ifndef MOIRAI_CONTAINERS_PRIORITY_QUEUE
#define MOIRAI_CONTAINERS_PRIORITY_QUEUE
#include <algorithm>
#include <type_traits>
#include <utility>
#include <vector>

namespace ambasta {

template<typename T, typename C>
class PriorityQueue : private C
{
private:
  std::vector<T> m_queue;

public:
  PriorityQueue(PriorityQueue const& other)
    : C(other)
  {
    m_queue = other.m_queue;
  }

  PriorityQueue(PriorityQueue&& other) noexcept(
    std::is_nothrow_move_constructible_v<C>)
    : C(std::move(other))
    , m_queue(std::move(other.m_queue))
  {}

  PriorityQueue& operator=(PriorityQueue&& other) noexcept(
    std::is_nothrow_move_assignable_v<C>)
  {
    C::operator=(std::move(other));
    m_queue = std::move(other.m_queue);
    return *this;
  }

  PriorityQueue& operator=(PriorityQueue const& other)
  {
    static_cast<C&>(*this) = static_cast<C const&>(other);
    m_queue = other.m_queue;
    return *this;
  }

  bool empty(void) const noexcept { return m_queue.empty(); }

  size_t size(void) const noexcept { return m_queue.size(); }

  size_t max_size(void) const noexcept { return m_queue.max_size(); }

  void clear(void) noexcept { m_queue.clear(); }

  T const& top(void) const
  {
    assert(not empty());
    return C::get_value(m_queue.front());
  }

  void push(T const& t)
  {
    m_queue.push_back(C::make_node(t));
    std::push_heap(m_queue.begin(), m_queue.end());
  }

  template<class... Args>
  void emplace(Args&&... args)
  {
    m_queue.emplace_back(C::make_node(std::forward(args)...));
    std::push_heap(m_queue.begin(), m_queue.end());
  }

  void pop(void)
  {
    assert(not empty());
    std::pop_heap(m_queue.begin(), m_queue.end());
    m_queue.pop_back();
  }

  void swap(PriorityQueue& other) noexcept(
    std::is_nothrow_move_constructible_v<C>and
      std::is_nothrow_move_assignable_v<C>)
  {
    C::swap(other);
    m_queue.swap(other.m_queue);
  }

  auto begin(void) const noexcept { return m_queue.begin(); }

  auto end(void) const noexcept { return m_queue.end(); }

  void reserve(size_t size) { m_queue.reserve(size); }

  auto operator<=>(const PriorityQueue&) const = default;
};

}

#endif
