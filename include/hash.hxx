#ifndef MOIRAI_HASH
#define MOIRAI_HASH

#include <functional>

class Route;
class Center;

namespace std {

template <> struct hash<Route> {
public:
  auto operator()(const Route &) const -> size_t;
};

template <> struct hash<Center> {
public:
  auto operator()(const Center &) const -> size_t;
};
} // namespace std

#endif
