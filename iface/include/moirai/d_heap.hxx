#include <algorithm>
#include <memory>
#include <moirai/priority_queue.hxx>
#include <utility>

namespace ambasta {
template <typename T, size_t arity, typename AllocatorT = std::allocator<T>,
          typename ComparatorT = std::less<T>, bool StableT = false,
          typename StabilityCounterT = uint64_t,
          bool SizeComplexityConstantT = true>
class DHeapParams
    : Params<T, AllocatorT, ComparatorT, StableT, StabilityCounterT> {};

class Noop {
public:
  template <typename T> static void run(T &, std::size_t) {}
};

template <typename AllocatorT> class ExtractAllocatorTypes {
  using size_type = typename std::allocator_traits<AllocatorT>::size_type;
  using difference_type =
      typename std::allocator_traits<AllocatorT>::difference_type;
  using reference = typename AllocatorT::value_type &;
  using const_reference = typename AllocatorT::value_type const &;
  using pointer = typename std::allocator_traits<AllocatorT>::pointer;
  using const_pointer =
      typename std::allocator_traits<AllocatorT>::const_pointer;
};

template <typename T, typename ContainerIteratorT, typename Extractor>
class StableHeapIterator {
private:
  T const &dereference() const { return Extractor::get_value(); }

public:
  StableHeapIterator(void) {}

  explicit StableHeapIterator(ContainerIteratorT const &iterator) {}
};

template <typename Heap> class OrderedIteratorDispatcher {
  using size_type = typename Heap::size_type;
  using internal_type = typename Heap::internal_type;
  using value_type = typename Heap::value_type;
  using super_t = typename Heap::super_t;

  static size_type max_index(const Heap *heap) { return heap->size() - 1; }

  static bool is_leaf(const Heap *heap, size_type index) {
    return heap->is_leaf(index);
  }

  static std::pair<size_type, size_type> get_child_nodes(const Heap *heap,
                                                         size_type index) {
    return heap->children(index);
  }

  static value_type const &get_value(internal_type const &arg) {
    return super_t::get_value(arg);
  }
};

template <typename T, typename AllocatorT, typename ComparatorT>
class Implementation : ExtractAllocatorTypes<AllocatorT> {
  using value_type = T;
  using size_type = typename ExtractAllocatorTypes<AllocatorT>::size_type;
  using value_compare = ComparatorT;
  using allocator_type =
      typename std::allocator_traits<AllocatorT>::allocator_type;

  // ordered_adaptor_iterator<const value_type, internal_type, DHeap,
  // allocator_type, typename MakeBaseHeap<T, ParameterT,
  // false>::type::internal_compare, OrderedIteratorDispatcher>;
  using ordered_iterator = int; // TODO

  // stable_heap_iterator<const value_type, container_iterator, typename
  // MakeBaseHeap<T, ParameterT, false>::type>
  using iterator = int; // TODO
  using const_iterator = iterator;
  typedef void *handle_type;
};

template <typename T, size_t arity, typename ParameterT = DHeapParams<T, arity>,
          class IndexUpdaterT = Noop>
class DHeap : private MakeBaseHeap<T, ParameterT, false>::type {
public:
  using value_type = T;
  using heap_base_maker = MakeBaseHeap<T, ParameterT, false>;
  using super_t = typename heap_base_maker::type;
  using internal_type = typename super_t::internal_type;
  using internal_type_allocator =
      typename std::allocator_traits<typename heap_base_maker::AllocatorT>::
          template rebind_alloc<internal_type>;
  using container_type = std::vector<internal_type, internal_type_allocator>;
  using index_updater = IndexUpdaterT;

  template <typename HeapL, typename HeapR> friend class HeapMergeEmulate;

  using ordered_iterator_dispatcher = OrderedIteratorDispatcher<DHeap>;
  using implementation = Implementation<T, internal_type_allocator,
                                        typename ParameterT::ComparatorT>;

public:
  using size_type = typename implementation::size_type;
  using difference_type = typename implementation::difference_type;
  using value_compare = typename implementation::value_compare;
  using allocator_type = typename implementation::allocator_type;
  using reference = typename implementation::reference;
  using const_reference = typename implementation::const_reference;
  using pointer = typename implementation::pointer;
  using const_pointer = typename implementation::const_pointer;
  using iterator = typename implementation::iterator;
  using const_iterator = typename implementation::const_iterator;
  using ordered_iterator = typename implementation::ordered_iterator;
  using handle_type = typename implementation::handle_type;

  static const bool is_stable = ParameterT::StableT;

private:
  static const unsigned int d = arity;
  container_type m_queue;

  void reset_index(size_type index, size_type updated_index) {
    assert(index < m_queue.size());
    index_updater::run(m_queue[index], updated_index);
  }

  static constexpr size_type first_child_index = [](size_type index) {
    return index * arity + 1;
  };

  size_type top_child_index(size_type index) const {
    const size_type first_index = first_child_index(index);
    using container_iterator = typename container_type::const_iterator;

    const container_iterator first_child = m_queue.begin() + first_index;
    const container_iterator end = m_queue.end();

    const size_type max_elements = std::distance(first_child, end);
    const container_iterator last_child =
        (max_elements > arity) ? first_child + arity : end;
    const container_iterator min_element = std::max_element(
        first_child, last_child, static_cast<super_t const &>(*this));
    return min_element;
  }

  static size_type parent_index(size_type index) { return (index - 1) / arity; }

  size_type last_child_index(size_type index) const {
    const size_type first_index = first_child_index(index);
    const size_type last_index = std::min(first_index + arity - 1, size() - 1);
    return last_index;
  }

  void sift_down(size_type index) {
    while (not leaf(index)) {
      size_type max_child_index = top_child_index(index);

      if (not super_t::operator()(m_queue[max_child_index], m_queue[index])) {
        reset_index(index, max_child_index);
        reset_index(max_child_index, index);
        std::swap(m_queue[max_child_index], m_queue[index]);
        index = max_child_index;
      } else
        return;
    }
  }

  void sift_up(size_type index) {
    while (index != 0) {
      size_type parent = parent_index(index);

      if (super_t::operator()(m_queue[parent], m_queue[index])) {
        reset_index(index, parent);
        reset_index(parent, index);
        std::swap(m_queue[parent], m_queue[index]);
        index = parent;
      } else
        return;
    }
  }

  bool leaf(size_type index) const {
    const size_t first_child = first_child_index(index);
    return first_child >= m_queue.size();
  }

  void update(size_type index) {
    if (index == 0) {
      sift_down(index);
      return;
    }
    size_type parent = parent_index(index);

    if (super_t::operator()(m_queue[parent], m_queue[index]))
      sift_up(index);
    else
      sift_down(index);
  }

  void erase(size_type index) {
    while (index != 0) {
      size_type parent = parent_index(index);
      reset_index(index, parent);
      reset_index(parent, index);
      std::swap(m_queue[parent], m_queue[index]);
      index = parent;
    }
    pop();
  }

  void increase(size_type index) { sift_up(index); }

  void decrease(size_type index) { sift_down(index); }

public:
  explicit DHeap(value_compare const &cmp = value_compare()) : super_t(cmp) {}

  DHeap(DHeap const &other) : super_t(other), m_queue(other.m_queue) {}

  DHeap(DHeap &&other)
      : super_t(std::move(other)), m_queue(std::move(other.m_queue)) {}

  DHeap &operator=(DHeap &&other) {
    super_t::operator=(std::move(other));
    m_queue = std::move(other.m_queue);
    return *this;
  }

  DHeap &operator=(DHeap const &other) {
    static_cast<super_t &>(*this) = static_cast<super_t const &>(other);
    m_queue = other.m_queue;
    return *this;
  }

  bool empty(void) const { return m_queue.empty(); }

  size_type size(void) const { return m_queue.size(); }

  size_type max_size(void) const { return m_queue.max_size(); }

  void clear(void) { m_queue.clear(); }

  allocator_type get_allocator(void) const { return m_queue.get_allocator(); }

  value_type const &top(void) const {
    assert(not empty());
    return super_t::get_value(m_queue.front());
  }

  void push(value_type const &value) {
    m_queue.push_back(super_t::make_node(value));
    reset_index(size() - 1, size() - 1);
    sift_up(m_queue.size() - 1);
  }

  template <class... Args> void emplace(Args &&...args) {
    m_queue.emplace_back(super_t::make_node(std::forward(args)...));
    reset_index(size() - 1, size() - 1);
    sift_up(m_queue.size() - 1);
  }

  void pop(void) {
    assert(not empty());
    std::swap(m_queue.front(), m_queue.back());
    m_queue.pop_back();

    if (m_queue.empty())
      return;
    reset_index(0, 0);
    sift_down(0);
  }

  void swap(DHeap &other) {
    super_t::swap(other);
    m_queue.swap(other.m_queue);
  }

  iterator begin(void) const { return iterator(m_queue.begin()); }

  iterator end(void) const { return iterator(m_queue.end()); }

  ordered_iterator ordered_begin(void) const {
    return ordered_iterator(0, this, super_t::get_internal_cmp());
  }

  ordered_iterator ordered_end(void) const {
    return ordered_iterator(size(), this, super_t::get_internal_cmp());
  }

  void reserve(size_type size) { m_queue.reserve(size); }

  value_compare const &value_comp(void) const { return super_t::value_comp(); }
};
} // namespace ambasta
