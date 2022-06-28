#ifndef MOIRAI_SOLVER_HXX
#define MOIRAI_SOLVER_HXX

#include "hash.hxx"
#include "transport_center.hxx"
#include "transport_edge.hxx"
#include <boost/graph/adjacency_list.hpp>

using namespace boost;

using Graph = adjacency_list<vecS, vecS, bidirectionalS,
                             std::shared_ptr<Center>, std::shared_ptr<Route>>;

template <class G> using VertexD = typename graph_traits<G>::vertex_descriptor;

template <class G> using EdgeD = typename graph_traits<G>::edge_descriptor;

class Segment {
private:
  std::shared_ptr<Center> mNode = nullptr;
  std::shared_ptr<Route> mOutbound = nullptr;
  datetime mDistance;

public:
  Segment(std::shared_ptr<Center>, std::shared_ptr<Route>, datetime);

  [[nodiscard]] auto node() const -> std::shared_ptr<Center>;

  [[nodiscard]] auto edge() const -> std::shared_ptr<Route>;

  [[nodiscard]] auto distance() const -> datetime;
};

template <typename Graph>
auto solve(const VertexD<Graph> &, const VertexD<Graph> &, datetime, datetime,
           const auto &, const auto &, const Graph &) -> std::vector<Segment>;

class Solver {

private:
  Graph mGraph;
  std::map<std::string_view, VertexD<Graph>> mNamedVertexMap;
  std::map<std::string_view, EdgeD<Graph>> mNamedEdgeMap;

public:
  auto node(const VertexD<Graph> &) const -> std::shared_ptr<Center>;

  auto node(std::shared_ptr<Center>) -> VertexD<Graph>;

  auto edge(const EdgeD<Graph> &) const -> std::shared_ptr<Route>;

  auto edge(std::string_view, std::string_view, std::shared_ptr<Route>)
      -> EdgeD<Graph>;

  /*
  template <typename FGraph>
  auto solve(const VertexD<FGraph> &, const VertexD<FGraph> &, datetime,
             datetime, const auto &, const auto &, const FGraph &) const
      -> std::vector<Segment>;
  */

  template <TraversalMode P, Vehicle V = Vehicle::AIR>
  [[nodiscard]] auto find_path(const VertexD<Graph> &, const VertexD<Graph> &,
                               datetime) const -> std::vector<Segment>;
};

#endif
