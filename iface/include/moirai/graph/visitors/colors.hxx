#ifndef GRAPH_VISITORS_COLORS
#define GRAPH_VISITORS_COLORS

#include <array>
#include <limits>
#include <memory>
#include <moirai/property_maps/concepts.hxx>
#include <moirai/property_maps/helpers.hxx>
#include <ranges>
#include <vector>

enum class Color { WHITE = 1, GRAY = 2, GREEN = 4, BLACK = 8 };

// read_write_property_map
template <PropertyMapConcept IndexMapT = IdentityPropertyMap>
class TwoBitColorMap {
  size_t m_size;
  IndexMapT m_index_map;
  std::vector<Color> m_data;

public:
  using value_type = Color;
  using reference = void;
  using key_type = typename IndexMapT::key_type;

  explicit TwoBitColorMap(size_t size, const IndexMapT &index_map = IndexMapT())
      : m_size(size), m_index_map(index_map), m_data(size) {}

  Color get(const typename IndexMapT::key_type key) {
    const typename IndexMapT::value_type index = m_index_map.get(key);
    assert(index < m_index_map.size());
    return m_data[index];
  }

  void put(const typename IndexMapT::key_type key, Color color) {
    const typename IndexMapT::value_type index = m_index_map.get(key);
    assert(index < m_index_map.size());
    m_data[index] = color;
  }
};
#endif
