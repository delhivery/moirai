#include "hash.hxx"
#include "transport_center.hxx"
#include "transport_edge.hxx"

namespace std {

auto hash<Route>::operator()(const Route &route) const -> size_t {
  return std::hash<std::string>()(route.code());
}

auto hash<Center>::operator()(const Center &center) const -> size_t {
  return std::hash<std::string>()(center.code());
}

} // namespace std
