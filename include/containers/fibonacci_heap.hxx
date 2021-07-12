#include <concepts>
#include <type_traits>

template<typename T>
concept FibonacciNode = requires(T t, bool B)
{
  {
    t.mark(B)
    } -> std::convertible_to<bool>;
  {
    t.unmark(B)
    } -> std::same_as<bool>;
};

template<FibonacciNode T>
constexpr bool truly_is_fibonnaci_node = true;

template<class N>
class Heap
{
  static_assert(truly_is_fibonnaci_node<N>);
};

template<FibonacciNode N>
class NoStaticHeap
{};
