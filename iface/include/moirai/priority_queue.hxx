#include <algorithm>
#include <iterator>
#include <memory>
#include <type_traits>
#include <vector>

namespace ambasta {
template <bool ConstantSizeT, typename SizeT> class SizeHolder {
private:
  SizeT m_size;

public:
  static const bool constant_time_size = ConstantSizeT;

  using size_type = SizeT;

  SizeHolder(void) noexcept : m_size(0) {}

  SizeHolder(SizeHolder &&other) noexcept : m_size(other.m_size) {
    other.m_size = 0;
  }

  SizeHolder(SizeHolder const &other) noexcept : m_size(other.m_size) {}

  SizeHolder &operator=(SizeHolder &&other) noexcept {
    m_size = other.m_size;
    other.m_size = 0;
    return *this;
  }

  SizeHolder &operator=(SizeHolder const &other) noexcept {
    m_size = other.m_size;
    return *this;
  }

  SizeT size() const noexcept { return m_size; }

  void size(SizeT size) noexcept { m_size = size; }

  void operator++() noexcept { ++m_size; }

  void operator++(SizeT size) noexcept { m_size += size; }

  void operator--() noexcept { --m_size; }

  void operator--(SizeT size) noexcept { m_size -= size; }

  void swap(SizeHolder &other) noexcept { std::swap(m_size, other.m_size); }
};

template <typename T, typename ComparatorT, bool SizeComplexityConstantT,
          typename StabilityCounterT = uint64_t, bool stable = false>
class BaseHeap : ComparatorT, SizeHolder<SizeComplexityConstantT, size_t> {
  using stability_counter_type = StabilityCounterT;
  using value_type = T;
  using internal_type = T;
  using size_holder_type = SizeHolder<SizeComplexityConstantT, size_t>;
  using value_compare = ComparatorT;
  using internal_cmp = ComparatorT;
  static const bool is_stable = stable;

  ComparatorT &m_compare_ref(void) { return *this; }

public:
  BaseHeap(ComparatorT const &comparator = ComparatorT())
      : ComparatorT(comparator) {}

  BaseHeap(BaseHeap &&other) noexcept(
      std::is_nothrow_move_constructible_v<ComparatorT>)
      : ComparatorT(std::move(static_cast<ComparatorT &>(other))),
        size_holder_type(std::move(static_cast<size_holder_type &>(other))) {}

  BaseHeap(BaseHeap const &other)
      : ComparatorT(static_cast<ComparatorT const &>(other)),
        size_holder_type(static_cast<size_holder_type const &>(other)) {}

  BaseHeap &operator=(BaseHeap &&other) noexcept(
      std::is_nothrow_move_assignable_v<ComparatorT>) {
    m_compare_ref().operator=(std::move(other.m_compare_ref()));
    size_holder_type::operator=(
        std::move(static_cast<size_holder_type &>(other)));
    return *this;
  }

  BaseHeap &operator=(BaseHeap const &other) {
    m_compare_ref().operator==(other.value_comp());
    size_holder_type::operator=(static_cast<size_holder_type const &>(other));
    return *this;
  }

  ComparatorT const &value_comp(void) const noexcept { return *this; }

  bool operator()(internal_type const &lhs, internal_type const &rhs) const {
    return value_comp().operator()(lhs, rhs);
  };

  internal_type make_node(T const &value) { return value; }

  T &&make_node(T &&value) { return std::forward<T>(value); }

  template <class... Args> internal_type make_node(Args &&...args) {
    return internal_type(std::forward(args)...);
  }

  static T &get_value(internal_type const &value) noexcept { return value; }

  ComparatorT const &get_internal_cmp(void) const noexcept {
    return value_comp();
  }

  void swap(BaseHeap &other) noexcept(
      std::is_nothrow_move_constructible_v<ComparatorT>
          and std::is_nothrow_move_assignable_v<ComparatorT>) {
    std::swap(m_compare_ref(), other.m_compare_ref());
    SizeHolder<SizeComplexityConstantT, size_t>::swap(other);
  }

  stability_counter_type get_stability_count(void) const noexcept { return 0; }

  void set_stability_count(stability_counter_type) noexcept {}

  template <typename HeapL, typename HeapR> friend class HeapMergeEmulate;
};

template <typename T, typename AllocatorT = std::allocator<T>,
          typename ComparatorT = std::less<T>, bool StableT = false,
          typename StabilityCounterT = uint64_t>
struct Params {
  static const bool is_stable = StableT;
};

template <typename T, typename Parameters = Params<T>,
          bool SizeComplexityConstantT = false>
class MakeBaseHeap {
  using ComparatorT = typename Parameters::ComparatorT;
  using AllocatorT = typename Parameters::AllocatorT;
  using StabilityCounterT = typename Parameters::StabilityCounterT;

  static const bool is_stable = Parameters::is_stable;

  using type = BaseHeap<T, ComparatorT, SizeComplexityConstantT,
                        StabilityCounterT, is_stable>;
};

template <typename T, typename ContainerIteratorT, typename ExtractorT>
class HeapIterator {};

template <typename HeapL, typename HeapR>
bool value_compare(HeapL const &lhs, HeapR const &rhs,
                   typename HeapL::value_type lval,
                   typename HeapR::value_type rval) {
  typename HeapL::value_compare const &comparator = lhs.value_comp();
  bool ret = comparator(lval, rval);
  assert(ret == rhs.value_comp()(lval, rval));
  return ret;
}

class HeapCompareIterative {
  template <typename HeapL, typename HeapR>
  bool operator()(HeapL const &lhs, HeapR const &rhs) {
    typename HeapL::size_type left_size = lhs.size();
    typename HeapR::size_type right_size = rhs.size();

    if (left_size < right_size)
      return true;

    if (left_size > right_size)
      return false;

    typename HeapL::ordered_iterator lhs_begin = lhs.ordered_begin();
    typename HeapL::ordered_iterator lhs_end = lhs.ordered_end();

    typename HeapR::ordered_iterator rhs_begin = rhs.ordered_begin();
    typename HeapR::ordered_iterator rhs_end = rhs.ordered_end();

    while (true) {
      if (value_compare(lhs, rhs, *lhs_begin, *rhs_begin))
        return true;

      if (value_compare(lhs, rhs, *rhs_begin, *lhs_begin))
        return false;

      ++lhs_begin;
      ++rhs_begin;

      if (lhs_begin == lhs_end and rhs_begin == rhs_end)
        return true;

      if (lhs_begin == lhs_end or rhs_begin == rhs_end)
        return false;
    }
  }
};

class HeapCompareCopy {
  template <typename HeapL, typename HeapR>
  bool operator()(HeapL const &lhs, HeapR const &rhs) {
    typename HeapL::size_type size_lhs = lhs.size();
    typename HeapR::size_type size_rhs = rhs.size();

    if (size_lhs < size_rhs)
      return true;

    if (size_lhs > size_rhs)
      return false;

    HeapL copy_lhs(lhs);
    HeapR copy_rhs(rhs);

    while (true) {
      if (value_compare(copy_lhs, copy_rhs, copy_lhs.top(), copy_rhs.top()))
        return true;

      if (value_compare(copy_lhs, copy_rhs, copy_rhs.top(), copy_lhs.top()))
        return false;

      copy_lhs.pop();
      copy_rhs.pop();

      if (copy_lhs.empty() and copy_rhs.empty())
        return false;
    }
  }
};

template <typename HeapL, typename HeapR>
bool value_equality(HeapL const &lhs, HeapR const &rhs,
                    typename HeapL::value_type lval,
                    typename HeapR::value_type rval) {
  typename HeapL::value_compare const &comparator = lhs.value_comp();

  bool ret = not(comparator(lval, rval)) and not(comparator(rval, lval));

  assert(ret == not(rhs.value_comp()(lval, rval)) and
         not(rhs.value_comp()(rval, lval)));
  return ret;
}

class HeapEquivalentIterator {
  template <typename HeapL, typename HeapR>
  bool operator()(HeapL const &lhs, HeapL const &rhs) {
    // assert that both are priorityqueues
    static_assert(std::is_same_v<typename HeapL::value_compare,
                                 typename HeapR::value_compare>);

    if (HeapL::constant_time_size and HeapR::constant_time_size)
      if (lhs.size() != rhs.size())
        return false;

    if (lhs.empty() and rhs.empty())
      return true;

    typename HeapL::ordered_iterator iter_lhs = lhs.ordered_begin();
    typename HeapL::ordered_iterator iter_lhs_end = lhs.ordered_end();

    typename HeapR::ordered_iterator iter_rhs = rhs.ordered_begin();
    typename HeapR::ordered_iterator iter_rhs_end = rhs.ordered_end();

    while (true) {
      if (not value_equality(lhs, rhs, *iter_lhs, *iter_rhs))
        return false;

      ++iter_lhs;
      ++iter_rhs;

      if (iter_lhs == iter_lhs_end and iter_rhs == iter_rhs_end)
        return true;

      if (iter_lhs == iter_lhs_end or iter_rhs == iter_rhs_end)
        return false;
    }
  }
};

class HeapEquivalentCopy {
  template <typename HeapL, typename HeapR>
  bool operator()(HeapL const &lhs, HeapL const &rhs) {
    // assert both are PriorityQueue-able heaps
    static_assert(std::is_same_v<typename HeapL::value_compare,
                                 typename HeapR::value_compare>);

    if (HeapL::constant_time_size and HeapR::constant_time_size)
      if (lhs.size() != rhs.size())
        return false;

    if (lhs.empty() and rhs.empty())
      return true;

    HeapL lhs_copy(lhs);
    HeapR rhs_copy(rhs);

    while (true) {
      if (not value_equality(lhs_copy, rhs_copy, lhs_copy.top(),
                             rhs_copy.top()))
        return false;
      lhs_copy.pop();
      rhs_copy.pop();

      if (lhs_copy.empty() and rhs_copy.empty())
        return true;

      if (lhs_copy.empty() or rhs_copy.empty())
        return false;
    }
  }
};

template <typename HeapL, typename HeapR>
bool heap_equality(HeapL const &lhs, HeapR const &rhs) {
  const bool use_ordered_iterators =
      HeapL::has_ordered_iterators and HeapR::has_ordered_iterators;

  using equivalence_check =
      std::conditional_t<use_ordered_iterators, HeapEquivalentIterator,
                         HeapEquivalentCopy>;

  equivalence_check eq_check;

  return eq_check(lhs, rhs);
}

template <typename HeapL, typename HeapR>
bool heap_compare(HeapL const &heap_l, HeapR const &heap_r) {
  const bool use_ordered_iterators =
      HeapL::has_ordered_iterators and HeapR::has_ordered_iterators;

  using compare_check =
      std::conditional_t<use_ordered_iterators, HeapCompareIterative,
                         HeapCompareCopy>;

  compare_check check_object;
  return check_object(heap_l, heap_r);
}
// Optionally provider
// Allocator, Comparator, StableT and StabilityCounterT
template <typename T, typename ParamsT = Params<T>>
class PriorityQueue : MakeBaseHeap<T, ParamsT, false> {
  using heap_base_maker = MakeBaseHeap<T, ParamsT, false>;
  using super_t = typename heap_base_maker::type;
  using internal_type = typename super_t::internal_type;
  using internal_type_allocator =
      typename std::allocator_traits<typename heap_base_maker::AllocatorT>::
          template rebind_alloc<internal_type>;
  using container_type = std::vector<internal_type, internal_type_allocator>;
  using size_type = typename container_type::size_type;
  using allocator_type = typename container_type::allocator_type;
  using const_reference = typename container_type::allocator_type::value_type;

  container_type m_queue;

public:
  using value_type = T;
  using value_compare = typename ParamsT::ComparatorT;

  static const bool constant_time_size = true;
  static const bool has_ordered_iterators = false;
  static const bool is_mergable = false;
  static const bool is_stable = heap_base_maker::is_stable;
  static const bool has_reserve = true;

  explicit PriorityQueue(value_compare const &comparator = value_compare())
      : super_t(comparator) {}

  PriorityQueue(PriorityQueue const &other)
      : super_t(other), m_queue(other.m_queue) {}

  PriorityQueue &operator=(PriorityQueue &&other) noexcept(
      std::is_nothrow_move_assignable_v<super_t>) {
    super_t::operator==(std::move(other));
    m_queue = std::move(other.m_queue);
    return *this;
  }

  PriorityQueue &operator=(PriorityQueue const &other) {
    static_cast<super_t &>(*this) = static_cast<super_t const &>(other);
    m_queue = other.m_queue;
    return *this;
  }

  bool empty(void) const noexcept { return m_queue.empty(); }

  size_type size(void) const noexcept { return m_queue.size(); }

  size_type max_size(void) const noexcept { return m_queue.max_size(); }

  void clear(void) noexcept { m_queue.clear(); }

  allocator_type get_allocator(void) const { return m_queue.get_allocator(); }

  const_reference top(void) const {
    assert(not empty());
    return super_t::get_value(m_queue.front());
  }

  void push(value_type &value) {
    m_queue.push_back(super_t::make_node(value));
    std::push_heap(m_queue.begin(), m_queue.end(),
                   static_cast<super_t const &>(*this));
  }

  template <class... Args> void emplace(Args &&...args) {
    m_queue.emplace_back(super_t::make_node(std::forward(args)...));
    std::push_heap(m_queue.begin(), m_queue.end(),
                   static_cast<super_t const &>(*this));
  }

  void pop(void) {
    assert(not empty());
    std::pop_heap(m_queue.begin(), m_queue.end(),
                  static_cast<super_t const &>(*this));
    m_queue.pop_back();
  }

  void swap(PriorityQueue &other) noexcept(
      std::is_nothrow_move_constructible_v<super_t>
          and std::is_nothrow_move_assignable_v<super_t>) {
    super_t::swap(other);
    m_queue.swap(other.m_queue);
  }

  auto begin(void) const noexcept { return std::iterator(m_queue.begin()); }

  auto end(void) const noexcept { return std::iterator(m_queue.end()); }

  void reserve(size_type size) { m_queue.reserve(size); }

  value_compare const &value_comp(void) const { return super_t::value_comp(); }

  template <typename HeapT> bool operator<(HeapT const &other) const {
    return heap_compare(*this, other);
  }

  template <typename HeapT> bool operator>(HeapT const &other) const {
    return heap_compare(other, *this);
  }

  template <typename HeapT> bool operator>=(HeapT const &other) const {
    return not operator<(other);
  }

  template <typename HeapT> bool operator==(HeapT const &other) const {
    return heap_equality(*this, other);
  }

  template <typename HeapT> bool operator!=(HeapT const &other) const {
    return not(*this == other);
  }
};

} // namespace ambasta
