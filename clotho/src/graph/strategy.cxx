#include <clotho/graph/strategy.hxx>
#include <memory>

namespace ambasta {
std::pair<const std::shared_ptr<Node>, bool>
Strategy::vertex(std::string_view label) const
{
  auto vertex = boost::vertex_by_label(label, *m_graph);

  if (vertex == boost::graph_traits<Graph>::null_vertex()) {
    return { nullptr, false };
  }
  return { (*m_graph)[vertex], true };
}

std::pair<const std::shared_ptr<Node>, bool>
Strategy::vertex(const VertexDescriptor& v) const
{
  return { (*m_graph)[v], true };
}

std::pair<const std::shared_ptr<Route>, bool>
Strategy::edge(const EdgeDescriptor& e) const
{
  return { (*m_graph)[e], true };
}

std::pair<const VertexDescriptor, bool>
Strategy::add_vertex(std::shared_ptr<Node> vp)
{
  const VertexDescriptor vertex = boost::add_vertex(vp->label(), vp, *m_graph);

  if (vertex == boost::graph_traits<Graph>::null_vertex())
    return { vertex, false };
  return { vertex, true };
}

std::pair<const EdgeDescriptor, bool>
Strategy::add_edge(std::string_view u_label,
                   std::string_view v_label,
                   std::shared_ptr<Route> ep)
{
  return boost::add_edge_by_label(u_label, v_label, ep, *m_graph);
}

std::pair<const EdgeDescriptor, bool>
Strategy::add_edge(std::shared_ptr<Node> u,
                   std::shared_ptr<Node> v,
                   std::shared_ptr<Route> ep)
{
  return add_edge(u->label(), v->label(), ep);
}

Strategy::Strategy()
{
  m_graph = std::make_shared<Graph>();
}

Strategy::Strategy(std::shared_ptr<Graph> graph)
  : m_graph(graph)
{}

Strategy::~Strategy()
{
  m_graph.reset();
}
}
