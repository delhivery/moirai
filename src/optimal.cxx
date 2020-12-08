#include <algorithm>   // for reverse
#include <iosfwd>      // for string
#include <queue>       // for priority_queue
#include <stdexcept>   // for invalid_argument
#include <string_view> // for string_view

#include <boost/graph/adjacency_list.hpp>        // for target, source
#include <boost/iterator/iterator_facade.hpp>    // for operator!=, operator++
#include <boost/pending/property.hpp>            // for lookup_one_property...
#include <boost/range/irange.hpp>                // for integer_iterator

#include "graph.hxx" // for Path, Vertex, Graph
#include "optimal.hxx"

bool Compare::operator()(const std::pair<Vertex, long> &first,
                         const std::pair<Vertex, long> &second) const {
  return first.second > second.second;
}

void Optimal::run_dijkstra(Vertex src, Vertex dst, DistanceMap &dmap,
                           PredecessorMap &pmap, long t_max, long inf,
                           long zero = 0) const {

  std::vector<int> visited(boost::num_vertices(graph));
  std::priority_queue<std::pair<Vertex, long>,
                      std::vector<std::pair<Vertex, long>>, Compare>
      bin_heap;

  for (auto vertices = boost::vertices(graph);
       vertices.first != vertices.second; vertices.first++) {
    Vertex vertex = *vertices.first;
    dmap[vertex] = inf;
    visited[vertex] = 0;
  }

  dmap[src] = zero;

  /*
  for (auto vertices = boost::vertices(g); vertices.first != vertices.second;
  vertices.first++) { Vertex vertex = *vertices.first;
      bin_heap.push(make_pair(vertex, dmap[vertex]));
  }*/
  bin_heap.push(std::make_pair(src, dmap[src]));

  while (!bin_heap.empty()) {
    auto current = bin_heap.top();
    Vertex source = current.first;
    bin_heap.pop();

    if (dmap[source] == inf) {
      break;
    }

    if (source == dst) {
      break;
    }

    auto out_edges = boost::out_edges(current.first, graph);

    for (auto eit = out_edges.first; eit != out_edges.second; eit++) {
      EdgeProperty edge = graph[*eit];
      Vertex target = boost::target(*eit, graph);

      long edge_cost = edge.weight(dmap[current.first], t_max);

      if (edge_cost != inf) {
        if (visited[target] == 0) {
          dmap[target] = edge_cost;
          pmap[target] = *eit;
          bin_heap.push(std::make_pair(target, dmap[target]));
          visited[target] = 1;
        } else if (visited[target] == 1) {

          if (edge_cost < dmap[target]) {
            dmap[target] = edge_cost;
            pmap[target] = *eit;
            bin_heap.push(std::make_pair(target, dmap[target]));
          }
        }
      }
    }
  }
}

std::vector<Path> Optimal::find_path(std::string_view src, std::string_view dst,
                                     long t_start, long t_max = P_L_INF) const {

  if (!has_vertex(src))
    throw std::invalid_argument("No matching source <> found");

  if (!has_vertex(dst))
    throw std::invalid_argument("No matching target <> found");

  Vertex source = add_vertex(src), target = add_vertex(dst);

  long zero = 0;
  long inf = P_L_INF;

  DistanceMap distances(boost::num_vertices(graph));
  PredecessorMap predecessors(boost::num_vertices(graph));

  run_dijkstra(source, target, distances, predecessors, t_max, inf, zero);

  std::vector<Path> path;

  Vertex current = target;
  Edge inbound;
  long departure = P_L_INF, expected_by = P_L_INF;
  bool first = true;

  do {
    auto distance = distances[current];

    if (distance == P_L_INF) {
      return path;
    }

    VertexProperty vprop = graph[current];

    if (first) {
      path.push_back(
          Path{graph[current].code, "", "", distance, expected_by, departure});
      first = false;
    } else {
      EdgeProperty eprop = graph[inbound];
      expected_by = distance + eprop.wait_time(distance);
      departure = expected_by + eprop._tap + eprop._top;
      path.push_back(Path{graph[current].code, graph[inbound].code,
                          graph[boost::target(inbound, graph)].code, distance,
                          expected_by, departure});
    }

    if (current == source) {
      break;
    }

    inbound = predecessors[current];
    current = boost::source(inbound, graph);
  } while (true);

  std::reverse(path.begin(), path.end());
  return path;
}
