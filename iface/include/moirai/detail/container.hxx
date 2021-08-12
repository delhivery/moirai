#ifndef MOIRAI_DETAIL_CONTAINER
#define MOIRAI_DETAIL_CONTAINER
#include <concepts>

namespace ambasta {
namespace detail {

template<typename C>
concept Container = requires(C& container, C const& const_container)
{
  typename C::value_type;
};

template<typename C, typename V>
concept container_of = Container<C> and std::same_as<V, typename C::value_type>;
}

}
#endif
