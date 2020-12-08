#include <algorithm>        // for for_each
#include <cassert>          // for assert
#include <cstdint>          // for int64_t
#include <filesystem>       // for path
#include <fstream>          // for string, ifstream
#include <initializer_list> // for initializer_list
#include <map>              // for map, operator!=
#include <string>           // for operator<, basic_...
#include <tuple>            // for tuple, get, tie
#include <utility>          // for pair
#include <vector>           // for vector

#include <nlohmann/detail/iterators/iter_impl.hpp> // for iter_impl
#include <nlohmann/json.hpp>                       // for basic_json<>::obj...
#include <nlohmann/json_fwd.hpp>                   // for json

#include <tbb/concurrent_hash_map.h> // for concurrent_hash_map

#include "graph.hxx"   // for Vertex, BaseGraph
#include "optimal.hxx" // for Optimal

const std::map<std::string, std::tuple<long, long>>
read_vertices(const std::filesystem::path file_path) {
  std::ifstream stream;
  stream.open(file_path);

  assert(stream.is_open());
  assert(!stream.fail());

  std::map<std::string, std::tuple<long, long>> center_map;
  auto data = nlohmann::json::parse(stream);

  for (auto &center : data) {
    nlohmann::json center_name, time_outbound, time_inbound;
    std::tie(center_name, time_outbound, time_inbound) =
        std::tie(center["center_name"], center["outbound"], center["inbound"]);

    center_map[center_name.get<std::string>()] = std::make_tuple(
        time_outbound.get<unsigned long>(), time_inbound.get<unsigned long>());
  }

  return center_map;
}

void solve(const std::filesystem::path file_path, const BaseGraph &graph,
           const std::map<std::string, std::string> code_map) {
  std::ifstream stream;
  stream.open(file_path);

  assert(stream.is_open());
  assert(!stream.fail());

  auto data = nlohmann::json::parse(stream);

  for (auto &record : data) {
    std::string bag = record["seal"].template get<std::string>(),
                wbn = record["waybill"].template get<std::string>(),
                src = record["source"].template get<std::string>(),
                tar = record["target"].template get<std::string>();
    long start = record["start"].template get<long>();

    auto path =
        graph.find_path(code_map.at(src), code_map.at(tar), start, P_L_INF);
  }
}

std::map<std::string, std::string> read_connections(
    const std::filesystem::path file_path, BaseGraph &graph,
    const std::map<std::string, std::tuple<long, long>> &center_map) {
  std::ifstream stream;
  stream.open(file_path);

  assert(stream.is_open());
  assert(!stream.fail());

  auto data = nlohmann::json::parse(stream);

  typedef tbb::concurrent_hash_map<std::string, std::string> CENTERS;
  typedef tbb::concurrent_hash_map<std::string, Vertex> VERTICES;
  std::map<std::string, std::string> centers, centers_rev;
  std::map<std::string, Vertex> vertices;
  // CENTERS centers;

  std::for_each(
      data.begin(), data.end(), [&centers, &centers_rev](auto const conn) {
        std::string source = conn["source"].template get<std::string>(),
                    source_name =
                        conn["source_name"].template get<std::string>(),
                    target = conn["destination"].template get<std::string>(),
                    target_name =
                        conn["destination_name"].template get<std::string>();
        centers[source] = source_name;
        centers[target] = target_name;
        centers_rev[source_name] = source;
        centers_rev[target_name] = target;
      });

  for (auto iter = centers.begin(); iter != centers.end(); ++iter) {
    Vertex vertex = graph.add_vertex(iter->first, iter->second);
    vertices[iter->first] = vertex;
  }

  std::int64_t counter = 0, prev = 0;
  std::for_each(data.begin(), data.end(), [&](auto const conn) {
    std::string source = conn["source"].template get<std::string>(),
                source_name = conn["source_name"].template get<std::string>(),
                target = conn["destination"].template get<std::string>(),
                target_name =
                    conn["destination_name"].template get<std::string>(),
                route_name = conn["route_uuid"].template get<std::string>();
    unsigned long departure = conn["departure"].template get<unsigned long>(),
                  duration = conn["interval"].template get<unsigned long>();

    unsigned long inbound = 0, outbound = 0;

    if (center_map.contains(source_name)) {
      inbound = std::get<0>(center_map.at(source_name));
    }

    if (center_map.contains(target_name)) {
      outbound = std::get<1>(center_map.at(target_name));
    }

    Vertex v_source = vertices.at(source), v_target = vertices.at(target);
    graph.add_edge(v_source, v_target, route_name, departure, duration, inbound,
                   0, outbound);
  });

  return centers_rev;
}

int main(int argc, char *argv[]) {
  Optimal solver;
  auto center_map = read_vertices("/home/amitprakash/moirai/data/centers.json");
  auto center_name_map = read_connections(
      "/home/amitprakash/moirai/data/routes.json", solver, center_map);
  solver.show();

  solve("/home/amitprakash/moirai/data/tests.json", solver, center_name_map);

  return 0;
}
