#ifndef MOIRAI_DETAIL_CONTAINER_STACK
#define MOIRAI_DETAIL_CONTAINER_STACK
#include <array>

namespace ambasta {
namespace detail {
template<typename T, auto S, auto R>
class Storage
{
  using parent = std::array<T, S.numel()>;
};
}
}
#endif
