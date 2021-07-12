#ifndef MOIRAI_GRAPH_EDGE
#define MOIRAI_GRAPH_EDGE

#include <compare>

namespace detail {

template<typename DirectionT, typename VertexT>
class EdgeBase
{
private:
  VertexT m_source, m_target;

public:
  inline EdgeBase(){};

  inline EdgeBase(VertexT source, VertexT target)
    : m_source(source)
    , m_target(target)
  {}

  auto operator<=>(const EdgeBase<DirectionT, VertexT>&) const = default;
};

template<typename DirectionT, typename VertexT>
class EdgeDescriptorImplementation : public EdgeBase<DirectionT, VertexT>
{
public:
  typedef void property_type;
  typedef EdgeBase<DirectionT, VertexT> Base;

private:
  property_type* m_property_ptr;

public:
  inline EdgeDescriptorImplementation()
    : m_property_ptr(0)
  {}

  inline EdgeDescriptorImplementation(VertexT source,
                                      VertexT target,
                                      const property_type* property_ptr)
    : Base(source, target)
    , m_property_ptr(const_cast<property_type*>(property_ptr))
  {}

  property_type* get_property() { return m_property_ptr; }

  auto operator<=>(
    const EdgeDescriptorImplementation<DirectionT, VertexT>&) const = default;
};
}
#endif
