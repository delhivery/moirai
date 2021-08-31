#ifndef MOIRAI_PROPERTY
#define MOIRAI_PROPERTY

class NoProperty {};

template <typename T> class NullProperty {
private:
  T m_value;
  NullProperty<T> m_base;

public:
  using value_type = T;
  using next_type = NullProperty;

  NullProperty(const T &value = T()) : m_value(value) {}

  NullProperty(const T &value, const NullProperty<T> &base)
      : m_value(value), m_base(base) {}
};

enum class InternalProperties {
  vertex_all_t,
  edge_all_t,
  graph_all_t,
  vertex_bundle_t,
  edge_bundle_t,
  graph_bundle_t,
};
#endif
