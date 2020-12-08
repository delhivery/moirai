#include <stdexcept> // for runtime_error
#include <string>    // for basic_string<>::__s...
#include <utility>   // for pair

#include <fmt/format.h> // for format

#include <boost/graph/adjacency_list.hpp>     // for add_edge, vertices
#include <boost/iterator/iterator_facade.hpp> // for operator!=, iterato...
#include <boost/range/irange.hpp>             // for integer_iterator

#include "graph.hxx"

Path::Path(std::string_view src, std::string_view conn, std::string_view dst,
           long arr, long mdep, long dep)
    : src(src), conn(conn), dst(dst), arr(arr), mdep(mdep), dep(dep) {}

VertexProperty::VertexProperty(std::string_view code, std::string_view name)
    : code(code), name(name) {}

EdgeProperty::EdgeProperty(const long _tip, const long _tap, const long _top,
                           std::string_view code)
    : code(code), _tip(_tip), _tap(_tap), _top(_top) {
  percon = true;
}

EdgeProperty::EdgeProperty(const long _dep, const long _dur, const long _tip,
                           const long _tap, const long _top,
                           std::string_view code)
    : _dep(_dep), _dur(_dur), _tip(_tip), _tap(_tap), _top(_top), code(code),
      dep(_dep - _tap - _top), dur(_dur + _tap + _top + _tip) {}

EdgeProperty::EdgeProperty(const size_t _index, EdgeProperty another) {
  _dep = another._dep;
  _dur = another._dur;
  _tip = another._tip;
  _tap = another._tap;
  _top = another._tip;
  code = another.code;
  dep = another.dep;
  dur = another.dur;
}

long EdgeProperty::wait_time(const long t_departure) const {
  if (percon)
    return 0;

  auto t_departure_durinal = t_departure % TIME_DURINAL;
  return (t_departure_durinal > dep)
             ? (TIME_DURINAL - t_departure_durinal + dep)
             : (dep - t_departure_durinal);
}

long EdgeProperty::weight(const long start, const long t_max) {
  if (percon) {
    return start + _tip + _tap + _top;
  }

  long time_total = wait_time(start) + dur + start;
  time_total = (time_total > t_max) ? P_L_INF : time_total;

  return time_total;
}

bool BaseGraph::has_vertex(std::string_view code) const {
  auto vertices = boost::vertices(graph);

  for (auto viter = vertices.first; viter != vertices.second; ++viter) {
    VertexProperty vprop = graph[*viter];

    if (vprop.code == code)
      return true;
  }
  return false;
}

Vertex BaseGraph::add_vertex(std::string_view code) const {
  auto vertices = boost::vertices(graph);

  for (auto vit = vertices.first; vit != vertices.second; ++vit) {
    VertexProperty vertex_property = graph[*vit];

    if (vertex_property.code == code) {
      return *vit;
    }
  }
  return -1;
}

Vertex BaseGraph::add_vertex(std::string_view code, std::string_view name) {
  auto vertices = boost::vertices(graph);

  for (auto vit = vertices.first; vit != vertices.second; ++vit) {
    VertexProperty vertex_property = graph[*vit];
    if (vertex_property.code == code) {
      return *vit;
    }
  }
  VertexProperty vertex_property{code, name};
  Vertex created = boost::add_vertex(vertex_property, graph);
  return created;
}

Edge BaseGraph::add_edge(std::string_view src, std::string_view src_name,
                         std::string_view dst, std::string_view dst_name,
                         std::string_view conn, const long tip, const long tap,
                         const long top) {
  Vertex source = add_vertex(src, src_name), target = add_vertex(dst, dst_name);

  EdgeProperty eprop{tip, tap, top, conn};
  auto created = boost::add_edge(source, target, eprop, graph);
  if (created.second) {
    return created.first;
  }
  throw std::runtime_error("Unable to create edge");
}

Edge BaseGraph::add_edge(std::string_view src, std::string_view dst,
                         std::string_view conn, const long tip, const long tap,
                         const long top) {
  Vertex source = add_vertex(src), target = add_vertex(dst);

  if (source == -1 or target == -1)
    throw std::runtime_error("Unable to create edge. Source or target missing");

  EdgeProperty eprop{tip, tap, top, conn};
  auto created = boost::add_edge(source, target, eprop, graph);
  if (created.second) {
    return created.first;
  }
  throw std::runtime_error("Unable to create edge");
}

Edge BaseGraph::add_edge(Vertex source, Vertex target, std::string_view conn,
                         const long tip, const long tap, const long top) {
  if (source == -1 or target == -1)
    throw std::runtime_error("Unable to create edge. Source or target missing");

  EdgeProperty eprop{tip, tap, top, conn};
  auto created = boost::add_edge(source, target, eprop, graph);
  if (created.second) {
    return created.first;
  }
  throw std::runtime_error("Unable to create edge");
}

Edge BaseGraph::add_edge(std::string_view src, std::string_view src_name,
                         std::string_view dst, std::string_view dst_name,
                         std::string_view conn, const long dep, const long dur,
                         const long tip, const long tap, const long top) {
  Vertex source = add_vertex(src, src_name), target = add_vertex(dst, dst_name);

  EdgeProperty eprop{dep, dur, tip, tap, top, conn};
  auto created = boost::add_edge(source, target, eprop, graph);

  if (created.second) {
    return created.first;
  }
  throw std::runtime_error("Unable to create edge");
}

Edge BaseGraph::add_edge(std::string_view src, std::string_view dst,
                         std::string_view conn, const long dep, const long dur,
                         const long tip, const long tap, const long top) {
  Vertex source = add_vertex(src), target = add_vertex(dst);

  if (source == -1 or target == -1)
    throw std::runtime_error("Unable to create edge. Source or target missing");
  EdgeProperty eprop{dep, dur, tip, tap, top, conn};
  auto created = boost::add_edge(source, target, eprop, graph);

  if (created.second) {
    return created.first;
  }
  throw std::runtime_error("Unable to create edge");
}

Edge BaseGraph::add_edge(Vertex source, Vertex target, std::string_view conn,
                         const long dep, const long dur, const long tip,
                         const long tap, const long top) {
  if (source == -1 or target == -1)
    throw std::runtime_error("Unable to create edge. Source or target missing");
  EdgeProperty eprop{dep, dur, tip, tap, top, conn};
  auto created = boost::add_edge(source, target, eprop, graph);

  if (created.second) {
    return created.first;
  }
  throw std::runtime_error("Unable to create edge");
}

std::string BaseGraph::show() const {
  return fmt::format("Graph<{}, {}>", boost::num_vertices(graph),
                     boost::num_edges(graph));
}
