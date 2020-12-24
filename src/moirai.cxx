#include "graph_helpers.hxx"
#include "solver.hxx"
#include "transportation.hxx"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <date/date.h>
#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <vector>

auto
find_paths(const Solver& solver,
           std::string wbn,
           std::string bwbn,
           std::string source_str,
           std::string target_str,
           std::string finols_str,
           int32_t start_time,
           int32_t reach_before)
{
  CLOCK start =
    CLOCK{ std::chrono::minutes{ 0 } } + std::chrono::minutes{ start_time };
  CLOCK by =
    CLOCK{ std::chrono::minutes{ 0 } } + std::chrono::minutes{ reach_before };

  const auto [source, has_src] = solver.add_node(source_str);
  const auto [target, has_tar] = solver.add_node(target_str);
  const auto [finols, has_fin] = solver.add_node(finols_str);
  Path forward_path, critical_path;
  nlohmann::json response;
  response["waybill"] = bwbn;
  response["package"] = wbn;

  if (has_src and has_tar and has_fin) {
    forward_path =
      solver.find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
        source, target, start);

    auto forward_wbn_path =
      solver.find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
        finols, target, by);

    if (forward_wbn_path.size() != 0) {
      CLOCK distance_critical = std::get<3>(forward_wbn_path[0]);

      critical_path =
        solver.find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
          target, source, distance_critical);
    }

    response["earliest"]["locations"] = {};

    std::shared_ptr<TransportEdge> inbound_edge = nullptr;

    for (auto idx = 0; idx < forward_path.size(); ++idx) {
      auto [edge_source, edge_target, edge_used, distance] = forward_path[idx];
      std::string node_name = edge_target->name;
      std::string node_code = edge_target->code;
      std::string arrival = date::format("%D %T", distance);

      nlohmann::json location = { { "name", node_name }, // edge source name
                                  { "code", node_code }, // edge source code
                                  { "arrival", arrival } };

      if (inbound_edge != nullptr) {
        location["route"]["code"] = inbound_edge->code;
        location["route"]["name"] = inbound_edge->name;
      }
      inbound_edge = edge_used;
      response["earliest"]["locations"].push_back(location);
    }
    {
      auto start_loc = solver.get_node(source);
      nlohmann::json location = { { "name", start_loc->name },
                                  { "code", start_loc->code },
                                  { "arrival", date::format("%D %T", start) } };

      if (inbound_edge != nullptr) {
        location["route"]["code"] = inbound_edge->code;
        location["route"]["name"] = inbound_edge->name;
      }
      response["earliest"]["locations"].push_back(location);
    }

    inbound_edge = nullptr;
    response["ultimate"]["locations"] = {};

    for (auto idx = 0; idx < forward_path.size(); ++idx) {
      auto [edge_source, edge_target, edge_used, distance] = forward_path[idx];
      nlohmann::json location = {
        { "name", edge_target->name }, // edge source name
        { "code", edge_target->code }, // edge source code
        { "arrival", date::format("%D %T", distance) }
      };

      if (inbound_edge != nullptr) {
        location["route"]["code"] = inbound_edge->code;
        location["route"]["name"] = inbound_edge->name;
      }
      inbound_edge = edge_used;
      response["ultimate"]["locations"].push_back(location);
    }
    {
      std::shared_ptr<TransportCenter> start_loc = nullptr;
      start_loc = solver.get_node(target);
      nlohmann::json location = { { "name", start_loc->name },
                                  { "code", start_loc->code },
                                  { "arrival", date::format("%D %T", start) } };
      if (inbound_edge != nullptr) {
        location["route"]["code"] = inbound_edge->code;
        location["route"]["name"] = inbound_edge->name;
      }
      response["ultimate"]["locations"].push_back(location);
    }
  }

  return response;
}

std::chrono::minutes
make_time(const std::string& str)
{
  std::regex split_time_regex(":");
  const std::vector<std::string> parts(
    std::sregex_token_iterator(str.begin(), str.end(), split_time_regex, -1),
    std::sregex_token_iterator());
  std::uint16_t time =
    std::atoi(parts[0].c_str()) * 60 + std::atoi(parts[1].c_str());

  return std::chrono::minutes(time);
}

void
solve_tests(const Solver& solver,
            const std::filesystem::path filename,
            const std::filesystem::path outfile)
{
  std::ifstream istream;
  istream.open(filename);

  assert(istream.is_open());
  assert(!istream.fail());

  auto data = nlohmann::json::parse(istream);

  nlohmann::json solutions;

  for (auto& test_case : data) {
    auto response = find_paths(solver,
                               test_case["wbn"].template get<std::string>(),
                               test_case["bag"].template get<std::string>(),
                               test_case["src"].template get<std::string>(),
                               test_case["tar"].template get<std::string>(),
                               test_case["fin"].template get<std::string>(),
                               test_case["sta"].template get<int>() + 330,
                               test_case["bef"].template get<int>() + 330);
    solutions.push_back(response);
  }
  std::ofstream ostream;
  ostream.open(outfile);

  assert(ostream.is_open());
  assert(!ostream.fail());

  ostream << solutions.dump();
  ostream.close();
}

std::vector<std::shared_ptr<TransportCenter>>
read_vertices(const std::filesystem::path filename)
{
  std::ifstream stream;
  stream.open(filename);

  assert(stream.is_open());
  assert(!stream.fail());

  std::vector<std::shared_ptr<TransportCenter>> centers;

  auto data = nlohmann::json::parse(stream);

  for (auto& center : data) {
    nlohmann::json center_name, center_code, time_outbound_carting,
      time_outbound_linehaul, time_inbound_carting, time_inbound_linehaul;

    std::tie(center_name,
             center_code,
             time_inbound_carting,
             time_inbound_linehaul,
             time_outbound_carting,
             time_outbound_linehaul) = std::tie(center["name"],
                                                center["code"],
                                                center["carting_inbound"],
                                                center["linehaul_inbound"],
                                                center["carting_outbound"],
                                                center["linehaul_outbound"]);

    TransportCenter transport_center(center_code, center_name);
    transport_center.set_latency<MovementType::CARTING, ProcessType::INBOUND>(
      DURATION(time_inbound_carting.get<unsigned long>()));
    transport_center.set_latency<MovementType::CARTING, ProcessType::OUTBOUND>(
      DURATION(time_outbound_carting.get<unsigned long>()));
    transport_center.set_latency<MovementType::LINEHAUL, ProcessType::INBOUND>(
      DURATION(time_inbound_linehaul.get<unsigned long>()));
    transport_center.set_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>(
      DURATION(time_outbound_carting.get<unsigned long>()));

    centers.push_back(std::make_shared<TransportCenter>(transport_center));
  }
  return centers;
}

std::vector<std::tuple<std::string,
                       std::string,
                       std::string,
                       std::string,
                       std::shared_ptr<TransportEdge>>>
read_connections(const std::filesystem ::path filename)
{
  std::ifstream stream;
  stream.open(filename);

  assert(stream.is_open());
  assert(!stream.fail());

  auto data = nlohmann::json::parse(stream);
  std::vector<std::tuple<std::string,
                         std::string,
                         std::string,
                         std::string,
                         std::shared_ptr<TransportEdge>>>
    edges;

  std::for_each(data.begin(), data.end(), [&edges](auto const route) {
    std::string uuid = route["route_schedule_uuid"].template get<std::string>();
    std::string name = route["name"].template get<std::string>();
    std::string route_type = route["route_type"].template get<std::string>();

    std::string reporting_time =
      route["reporting_time"].template get<std::string>();
    TIME_OF_DAY offset =
      std::chrono::duration_cast<TIME_OF_DAY>(make_time(reporting_time));

    auto stops = route["halt_centers"];

    if (!stops.is_array()) {
      std::cout << "Halt is not an array" << std::endl;
    }

    for (int i = 0; i < stops.size(); ++i) {
      for (int j = i + 1; j < stops.size(); ++j) {

        auto source = stops[i];
        auto target = stops[j];
        std::string departure = source["rel_etd"].template get<std::string>();
        std::string arrival = target["rel_eta"].template get<std::string>();

        auto debug_arrival = make_time(arrival);
        auto debug_departure = make_time(departure);
        auto debug_offset = offset;

        TIME_OF_DAY t_departure(
          offset +
          std::chrono::duration_cast<TIME_OF_DAY>(make_time(departure)));
        DURATION t_duration(
          std::chrono::duration_cast<TIME_OF_DAY>(make_time(arrival)) -
          std::chrono::duration_cast<TIME_OF_DAY>(make_time(departure)));

        if (t_departure.count() < 0 || t_duration.count() < 0) {
          std::cout
            << fmt::format(
                 "Invalid connection added {}.{} Arr: {} Dep: {} Off: {}",
                 uuid,
                 i * stops.size() + j - 1,
                 debug_arrival.count(),
                 debug_departure.count(),
                 debug_offset.count())
            << std::endl;

          assert(t_departure.count() > 0);
          assert(t_duration.count() > 0);
        }

        TransportEdge edge{
          fmt::format("{}.{}", uuid, i),
          name,
          t_departure,
          t_duration,
          route_type == "air" ? VehicleType::AIR : VehicleType::SURFACE,
          route_type == "carting" ? MovementType::CARTING
                                  : MovementType::LINEHAUL,
        };
        std::string source_center_code =
          source["center_code"].template get<std::string>();
        std::string source_center_name =
          source["center_name"].template get<std::string>();
        std::string target_center_code =
          target["center_code"].template get<std::string>();
        std::string target_center_name =
          target["center_name"].template get<std::string>();

        edges.push_back(std::make_tuple(source_center_code,
                                        source_center_name,
                                        target_center_code,
                                        target_center_name,
                                        std::make_shared<TransportEdge>(edge)));
      }
    }
  });

  return edges;
}

int
main(int argc, char* argv[])
{
  Solver solver;
  std::filesystem::path center_filepath{
    "/home/amitprakash/moirai/fixtures/centers.pretty.json"
  };

  std::filesystem::path edges_filepath{
    "/home/amitprakash/moirai/fixtures/routes.pretty.json"
  };

  std::filesystem::path tests_filepath{
    "/home/amitprakash/moirai/fixtures/tests.pretty.json"
  };

  std::filesystem::path outcomes_filepath{
    "/home/amitprakash/moirai/build/outputs.json"
  };

  if (argc > 1) {
    center_filepath = argv[1];
  }

  if (argc > 2) {
    edges_filepath = argv[2];
  }

  if (argc > 3) {
    tests_filepath = argv[3];
  }

  if (argc > 4) {
    outcomes_filepath = argv[4];
  }

  for (const auto& center : read_vertices(center_filepath)) {
    solver.add_node(center);
  }

  for (const auto& edge : read_connections(edges_filepath)) {
    std::string source = std::get<0>(edge);
    std::string source_name = std::get<1>(edge);
    std::string target = std::get<2>(edge);
    std::string target_name = std::get<3>(edge);
    std::shared_ptr<TransportEdge> e = std::get<4>(edge);

    auto src = solver.add_node(source);
    auto tar = solver.add_node(target);

    TransportCenter s, t;

    if (!src.second) {
      s = TransportCenter{ source, source_name };
      s.set_latency<MovementType::CARTING, ProcessType::INBOUND>(DURATION(0));
      s.set_latency<MovementType::LINEHAUL, ProcessType::INBOUND>(DURATION(0));
      s.set_latency<MovementType::CARTING, ProcessType::OUTBOUND>(DURATION(0));
      s.set_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>(DURATION(0));
      src = solver.add_node(std::make_shared<TransportCenter>(s));
    }

    if (!tar.second) {
      t = TransportCenter{ target, target_name };
      t.set_latency<MovementType::CARTING, ProcessType::INBOUND>(DURATION(0));
      t.set_latency<MovementType::LINEHAUL, ProcessType::INBOUND>(DURATION(0));
      t.set_latency<MovementType::CARTING, ProcessType::OUTBOUND>(DURATION(0));
      t.set_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>(DURATION(0));
      tar = solver.add_node(std::make_shared<TransportCenter>(t));
    }

    solver.add_edge(src.first, tar.first, e);
  }

  solve_tests(solver, tests_filepath, outcomes_filepath);
}
