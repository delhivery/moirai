#include "solvers/fastest_forward.hxx"
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/property_map/function_property_map.hpp>

namespace moirai {
namespace solver {
namespace detail {
std::chrono::time_point<std::chrono::system_clock> Combine::operator()(
    const std::chrono::time_point<std::chrono::system_clock> current_time,
    const int16_t minutes_duration) const {

  return current_time + std::chrono::minutes{minutes_duration};
}
} // namespace detail
} // namespace solver
} // namespace moirai
