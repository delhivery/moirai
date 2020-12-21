#include "graph_helpers.hxx"
#include "solver.hxx"
#include "transportation.hxx"
#include <cassert>
#include <chrono>
#include <date/date.h>
#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <vector>

auto
make_time(const std::string& str)
{
  std::chrono::seconds t;
  std::stringstream is;
  is << str;
  is >> date::parse("%T", t);
  return t;
}

std::vector<TransportCenter>
read_vertices(const std::filesystem::path filename)
{
  std::ifstream stream;
  stream.open(filename);

  assert(stream.is_open());
  assert(!stream.fail());

  std::vector<TransportCenter> centers;

  auto data = nlohmann::json::parse(stream);

  for (auto& center : data) {
    nlohmann::json center_name, center_code, time_outbound_carting,
      time_outbound_linehaul, time_inbound_carting, time_inbound_linehaul;

    std::tie(center_name,
             center_code,
             time_inbound_carting,
             time_inbound_linehaul,
             time_outbound_carting,
             time_outbound_linehaul) = std::tie(center["center_name"],
                                                center["center_code"],
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

    centers.push_back(transport_center);
  }
  return centers;
}

std::vector<
  std::tuple<std::string, std::string, std::string, std::string, TransportEdge>>
read_connections(const std::filesystem ::path filename)
{
  std::ifstream stream;
  stream.open(filename);

  assert(stream.is_open());
  assert(!stream.fail());

  auto data = nlohmann::json::parse(stream);
  std::vector<
    std::
      tuple<std::string, std::string, std::string, std::string, TransportEdge>>
    edges;

  std::for_each(data.begin(), data.end(), [&edges](auto const route) {
    std::string uuid = route["route_schedule_uuid"].template get<std::string>();
    std::string name = route["name"].template get<std::string>();
    std::string route_type = route["route_type"].template get<std::string>();

    std::string reporting_time =
      route["reporting_time"].template get<std::string>();
    TIME_OF_DAY offset =
      std::chrono::duration_cast<TIME_OF_DAY>(make_time(reporting_time));
    std::size_t halts = route["halts"].size();

    for (int i = 0; i < halts - 1; ++i) {
      auto source = route["halts"][i];
      auto target = route["halts"][i + 1];
      std::string departure = source["rel_etd"].template get<std::string>();
      std::string arrival = target["rel_eta"].template get<std::string>();

      TIME_OF_DAY t_departure(
        offset + std::chrono::duration_cast<TIME_OF_DAY>(make_time(departure)));
      DURATION t_duration(
        std::chrono::duration_cast<TIME_OF_DAY>(make_time(arrival)) -
        std::chrono::duration_cast<TIME_OF_DAY>(make_time(departure)));

      TransportEdge edge{
        fmt::format("{}.{}", uuid, i),
        name,
        t_departure,
        t_duration,
        route_type == "Air" ? VehicleType::AIR : VehicleType::SURFACE,
        route_type == "Carting" ? MovementType::CARTING
                                : MovementType::LINEHAUL,
      };

      edges.push_back(
        std::make_tuple(source["center_code"].template get<std::string>(),
                        source["center_name"].template get<std::string>(),
                        target["center_code"].template get<std::string>(),
                        target["center_name"].template get<std::string>(),
                        edge));
    }
  });

  return edges;
}

int
main()
{
  Solver solver;

  for (const auto& center :
       read_vertices("/home/amitprakash/moirai/data/centers.json")) {
    solver.add_node(center);
  }

  for (const auto& edge :
       read_connections("/home/amitprakash/moirai/fixtures/routes.json")) {

    std::string source = std::get<0>(edge);
    std::string source_name = std::get<1>(edge);
    std::string target = std::get<2>(edge);
    std::string target_name = std::get<3>(edge);

    TransportEdge e = std::get<4>(edge);

    auto src = solver.add_node(source);
    auto tar = solver.add_node(target);

    TransportCenter s, t;

    if (!src.second) {
      s = TransportCenter{ source, source_name };
      s.set_latency<MovementType::CARTING, ProcessType::INBOUND>(DURATION(0));
      s.set_latency<MovementType::LINEHAUL, ProcessType::INBOUND>(DURATION(0));
      s.set_latency<MovementType::CARTING, ProcessType::OUTBOUND>(DURATION(0));
      s.set_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>(DURATION(0));
      src = solver.add_node(s);
    }

    if (!tar.second) {
      t = TransportCenter{ source, source_name };
      t.set_latency<MovementType::CARTING, ProcessType::INBOUND>(DURATION(0));
      t.set_latency<MovementType::LINEHAUL, ProcessType::INBOUND>(DURATION(0));
      t.set_latency<MovementType::CARTING, ProcessType::OUTBOUND>(DURATION(0));
      t.set_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>(DURATION(0));
      tar = solver.add_node(t);
    }

    solver.add_edge(src.first, tar.first, e);
  }
}
