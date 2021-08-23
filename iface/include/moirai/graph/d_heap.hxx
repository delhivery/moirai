#include <cstddef>
#include <moirai/graph/property_map_concepts.hxx>
#include <ranges>
#include <vector>

template <typename T, size_t D, PropertyMapConcept IndexPropertyMapT,
          PropertyMapConcept DistanceMapT, typename ContainerT = std::vector<T>>
class DHeap {
  static_assert(D >= 2);

private:
  ContainerT m_data;
  DistanceMapT m_distances;
  IndexPropertyMapT m_indices;

public:
  using size_type = typename ContainerT::size_type;
  using value_type = T;
  using key_type = typename DistanceMapT::key_type;

private:
  using distance_type = typename DistanceMapT::value_type;

  static size_type constexpr parent = [](size_type index) {
    return (index - 1) / D;
  };

  bool compare_indirect(const value_type &lhs, const value_type &rhs) const {
    return get(m_distances, lhs) < get(m_distances, rhs);
  }

  void verify_heap() const {
#if 0
    // This is a very expensive test. Disable by default

    for (size_type idx = 1; idx < m_data.size(); ++idx) {
      assert(compare_indirect(m_data[idx], m_data[parent(idx)]),
             "Element is smaller than its parent");
    }
#endif
  }

  void preserve_heap_property_up(size_type index) {
    size_type original_index = index;
    size_type levels_moved = 0;

    if (index == 0)
      return;
    value_type current = m_data[index];
    distance_type distance_current = get(m_distances, current);
    while (true) {

      if (index == 0)
        break;
      size_type index_parent = parent(index);
      value_type value_parent = m_data[index_parent];
      if (distance_current < get(m_distances, value_parent)) {
        ++levels_moved;
        index = index_parent;
        continue;
      } else
        break;
    }
    index = original_index;

    while (levels_moved > 0) {
      size_type index_parent = parent(index);
      value_type value_parent = m_data[index_parent];
      put(m_indices, value_parent, index);
      m_data[index] = value_parent;
      index = index_parent;
      levels_moved--;
    }

    m_data[index] = current;
    put(m_indices, current, index);
    verify_heap();
  }

  void preserve_heap_property_down() {
    if (m_data.empty())
      return;
    size_type index = 0;
    value_type current = m_data[0];
    distance_type distance_current = get(m_distances, current);
    size_type heap_size = m_data.size();
    value_type *data_ptr = &m_data[0];

    while (true) {
      size_type index_first_child = child(index, 0);

      if (index_first_child > heap_size)
        break;
      value_type *child_base_ptr = data_ptr + index_first_child;
      size_type index_child_smallest = 0;
      distance_type distance_child_smallest =
          get(m_distances, child_base_ptr[index_child_smallest]);

      if (index_first_child + D <= heap_size) {
        for (size_t idx : std::ranges::iota_view{1, D}) {
          value_type value_idx = child_base_ptr[idx];
          distance_type distance_idx = get(m_distances, value_idx);

          if (distance_idx < distance_child_smallest) {
            index_child_smallest = idx;
            distance_child_smallest = distance_idx;
          }
        }
      } else {
        for (size_t idx :
             std::ranges::iota_view(1, heap_size - index_first_child)) {
          distance_type distance_idx = get(m_distances, child_base_ptr[idx]);
          if (distance_idx < distance_child_smallest) {
            index_child_smallest = idx;
            distance_child_smallest = distance_idx;
          }
        }
      }
    }
  }

public:
  DHeap(DistanceMapT distances, IndexPropertyMapT indices,
        const ContainerT &data = ContainerT())
      : m_data(data), m_distances(distances), m_indices(indices) {}

  size_type size() const { return m_data.size(); }

  bool empty() const { return m_data.empty(); }

  void push(const value_type &value) {
    size_type index = m_data.size();
    m_data.push_back(value);
    put(m_indices, value, index);
    preserve_heap_property_up(index);
    verify_heap();
  }

  value_type &top() {
    assert(not this->empty());
    return m_data[0];
  }

  const value_type &top() const {
    assert(not this->empty());
    return m_data[0];
  }

  void pop() {
    static_assert(not this->empty());
    put(m_indices, m_data[0], (size_type)(-1));

    if (m_data.size() != 1) {
      m_data[0] = m_data.back();
      put(m_indices, m_data[0], (size_type)(0));
      m_data.pop_back();
      preserve_heap_property_down();
      verify_heap();
    } else
      m_data.pop_back();
  }
};
