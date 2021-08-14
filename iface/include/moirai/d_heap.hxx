#include <memory>
#include <moirai/priority_queue.hxx>

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

template <typename T, size_t arity, typename ParameterT = DHeapParams<T, arity>,
          class IndexUpdaterT = Noop>
class DHeap : private MakeBaseHeap<T, ParameterT, false>::type {
  using heap_base_maker = MakeBaseHeap<T, ParameterT, false>;
  using super_t = typename heap_base_maker::internal_type;
};
} // namespace ambasta
