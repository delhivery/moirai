#include "solver_wrapper.hxx"
#include "date_utils.hxx"
#include "transportation.hxx"
#include <Poco/Util/ServerApplication.h>
#include <algorithm>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#ifdef __cpp_lib_format
#include <format>
#else
#include <fmt/core.h>
namespace std {
using fmt::format;
};
#endif

SolverWrapper::SolverWrapper(
  moodycamel::ConcurrentQueue<std::string>* load_queue,
  moodycamel::ConcurrentQueue<std::string>* solution_queue)
  : load_queue(load_queue)
  , solution_queue(solution_queue)
{}

std::vector<std::shared_ptr<TransportCenter>>
SolverWrapper::read_vertices(const std::filesystem::path& filename)
{
  std::ifstream stream;
  stream.open(filename);

  assert(stream.is_open());
  assert(!stream.fail());

  std::vector<std::shared_ptr<TransportCenter>> centers;

  auto data = nlohmann::json::parse(stream);

  for (auto& center : data) {
    nlohmann::json center_code, time_outbound_carting, time_outbound_linehaul,
      time_inbound_carting, time_inbound_linehaul;

    std::tie(center_code,
             time_inbound_carting,
             time_inbound_linehaul,
             time_outbound_carting,
             time_outbound_linehaul) = std::tie(center["code"],
                                                center["carting_inbound"],
                                                center["linehaul_inbound"],
                                                center["carting_outbound"],
                                                center["linehaul_outbound"]);

    TransportCenter transport_center(center_code);
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

std::vector<
  std::tuple<std::string, std::string, std::shared_ptr<TransportEdge>>>
SolverWrapper::read_connections(const std::filesystem ::path& filename)
{
  std::ifstream stream;
  stream.open(filename);

  assert(stream.is_open());
  assert(!stream.fail());

  auto data = nlohmann::json::parse(stream);
  std::vector<
    std::tuple<std::string, std::string, std::shared_ptr<TransportEdge>>>
    edges;

  std::for_each(data.begin(), data.end(), [&edges](auto const route) {
    std::string uuid = route["route_schedule_uuid"].template get<std::string>();
    std::string name = route["name"].template get<std::string>();
    std::string route_type = route["route_type"].template get<std::string>();

    std::string reporting_time =
      route["reporting_time"].template get<std::string>();
    TIME_OF_DAY offset = std::chrono::duration_cast<TIME_OF_DAY>(
      time_string_to_time(reporting_time));

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

        auto debug_arrival = time_string_to_time(arrival);
        auto debug_departure = time_string_to_time(departure);
        auto debug_offset = offset;

        TIME_OF_DAY t_departure(offset +
                                std::chrono::duration_cast<TIME_OF_DAY>(
                                  time_string_to_time(departure)));
        DURATION t_duration(std::chrono::duration_cast<TIME_OF_DAY>(
                              time_string_to_time(arrival)) -
                            std::chrono::duration_cast<TIME_OF_DAY>(
                              time_string_to_time(departure)));

        if (t_departure.count() < 0 || t_duration.count() < 0) {
          std::cout
            << fmt::format(
                 "Invalid connection added {}.{} Arr: {} Dep: {} Off: {}",
                 uuid,
                 i * (stops.size() - 1) + j - i - 1,
                 debug_arrival.count(),
                 debug_departure.count(),
                 debug_offset.count())
            << std::endl;

          assert(t_departure.count() > 0);
          assert(t_duration.count() > 0);
        }

        TransportEdge edge{
          fmt::format("{}.{}", uuid, i * (stops.size() - 1) + j - i - 1),
          name,
          t_departure,
          t_duration,
          route_type == "air" ? VehicleType::AIR : VehicleType::SURFACE,
          route_type == "carting" ? MovementType::CARTING
                                  : MovementType::LINEHAUL,
        };
        std::string source_center_code =
          source["center_code"].template get<std::string>();
        std::string target_center_code =
          target["center_code"].template get<std::string>();

        edges.push_back(std::make_tuple(source_center_code,
                                        target_center_code,
                                        std::make_shared<TransportEdge>(edge)));
      }
    }
  });

  return edges;
}

void
to_lower(std::string& input_string)
{
  std::transform(input_string.begin(),
                 input_string.end(),
                 input_string.begin(),
                 [](unsigned char c) { return std::tolower(c); });
}

auto
SolverWrapper::find_paths(
  std::string bag,
  std::string bag_source,
  std::string bag_target,
  int32_t bag_start,
  std::vector<std::tuple<std::string, int32_t, std::string>>& packages)
{
  CLOCK ZERO = CLOCK{ std::chrono::minutes{ 0 } };
  CLOCK start = ZERO + std::chrono::minutes{ bag_start };
  CLOCK bag_pdd = CLOCK::max();
  to_lower(bag_source);
  to_lower(bag_target);
  const auto [source, has_source] = solver.add_node(bag_source);
  const auto [target, has_target] = solver.add_node(bag_target);

  if (!has_source || !has_target) {
    return nlohmann::json({});
  }

  Path solution_earliest =
    solver.find_path<PathTraversalMode::FORWARD, VehicleType::SURFACE>(
      source, target, start);
  Path solution_ultimate;

  if (solution_earliest.size() > 0) {

    CLOCK bag_earliest_pdd = std::get<3>(solution_earliest[0]);

    for (auto const& package : packages) {
      std::string package_target_string = std::get<0>(package);
      to_lower(package_target_string);
      const auto [package_target, has_package_target] =
        solver.add_node(package_target_string);

      if (has_package_target) {
        Path pkg_reverse_path =
          solver.find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
            package_target,
            target,
            ZERO + std::chrono::minutes(std::get<1>(package)));

        if (pkg_reverse_path.size() > 0) {
          CLOCK package_pdd = std::get<3>(pkg_reverse_path[0]);

          if (package_pdd < bag_pdd and package_pdd >= bag_earliest_pdd) {
            bag_pdd = package_pdd;
          }
        }
      }
    }

    if (bag_pdd == CLOCK::max())
      solution_ultimate = solution_earliest;
    else
      solution_ultimate =
        solver.find_path<PathTraversalMode::REVERSE, VehicleType::SURFACE>(
          target, source, bag_pdd);
  } else
    solution_ultimate = solution_earliest;

  nlohmann::json response;
  response["_id"] = bag;
  response["waybill"] = bag;
  response["package"] = bag;

  if (packages.size() > 0) {
    response["package"] = std::get<2>(packages[0]);
  }

  response["earliest"]["locations"] = {};
  response["ultimate"]["locations"] = {};

  std::shared_ptr<TransportEdge> outbound_edge = nullptr;

  for (auto idx = 0; idx < solution_earliest.size(); ++idx) {
    auto [current_node, previous_node, inbound_edge, distance_current] =
      solution_earliest[idx];

    std::string current_node_code = current_node->code;
    std::string current_node_arrival = date::format("%D %T", distance_current);
    nlohmann::json location_entry = { { "code", current_node_code },
                                      { "arrival", current_node_arrival } };

    if (outbound_edge != nullptr) {
      location_entry["departure"] = date::format(
        "%D %T", get_departure(distance_current, outbound_edge->departure));
      location_entry["route"] =
        outbound_edge->code.substr(0, outbound_edge->code.find('.'));
    }
    response["earliest"]["locations"].push_back(location_entry);
    outbound_edge = inbound_edge;
  }
  {
    auto current_node = solver.get_node(source);
    nlohmann::json location_entry = { { "code", solver.get_node(source)->code },
                                      { "arrival",
                                        date::format("%D %T", start) } };

    if (outbound_edge != nullptr) {
      location_entry["departure"] =
        date::format("%D %T", get_departure(start, outbound_edge->departure));
      location_entry["route"] =
        outbound_edge->code.substr(0, outbound_edge->code.find('.'));
    }
    response["earliest"]["locations"].push_back(location_entry);
    std::reverse(response["earliest"]["locations"].begin(),
                 response["earliest"]["locations"].end());
    response["earliest"]["first"] = location_entry;
  }
  std::shared_ptr<TransportEdge> inbound_edge = nullptr;

  for (auto idx = 0; idx < solution_ultimate.size(); ++idx) {
    auto [edge_source, edge_target, edge_used, distance] =
      solution_ultimate[idx];
    std::string code = edge_target->code;
    std::string arrival = date::format("%D %T", distance);
    inbound_edge = edge_used;

    nlohmann::json location = { { "code", code }, { "arrival", arrival } };

    if (inbound_edge != nullptr) {
      location["route"] =
        inbound_edge->code.substr(0, inbound_edge->code.find('.'));
      location["departure"] =
        date::format("%D %T", get_departure(distance, inbound_edge->departure));
    }
    response["ultimate"]["locations"].push_back(location);
  }
  {
    auto start_location = solver.get_node(source);
    nlohmann::json location = { { "code", start_location->code },
                                { "arrival", date::format("%D %T", start) } };
    if (inbound_edge != nullptr) {
      location["route"] =
        inbound_edge->code.substr(0, inbound_edge->code.find("."));
      location["departure"] =
        date::format("%D %T", get_departure(start, inbound_edge->departure));
    }
    response["ultimate"]["locations"].push_back(location);
    response["ultimate"]["first"] = location;
  }

  response["pdd"] = date::format("%D %T", bag_pdd);
  return response;
}

void
SolverWrapper::run()
{
  Poco::Util::Application& app = Poco::Util::Application::instance();
  app.logger().debug("Initializing solver");
  std::filesystem::path center_filepath{
    "/home/amitprakash/moirai/fixtures/centers.pretty.json"
  };

  std::filesystem::path edges_filepath{
    "/home/amitprakash/moirai/fixtures/routes.utcized.json"
  };

  app.logger().debug("Adding vertices");
  for (const auto& center : read_vertices(center_filepath)) {
    solver.add_node(center);
  }

  app.logger().debug("Adding edges");
  for (const auto& edge : read_connections(edges_filepath)) {
    std::string source = std::get<0>(edge);
    std::string target = std::get<1>(edge);
    std::shared_ptr<TransportEdge> e = std::get<2>(edge);

    auto src = solver.add_node(source);
    auto tar = solver.add_node(target);

    TransportCenter s, t;

    if (!src.second) {
      s = TransportCenter{ source };
      s.set_latency<MovementType::CARTING, ProcessType::INBOUND>(DURATION(0));
      s.set_latency<MovementType::LINEHAUL, ProcessType::INBOUND>(DURATION(0));
      s.set_latency<MovementType::CARTING, ProcessType::OUTBOUND>(DURATION(0));
      s.set_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>(DURATION(0));
      src = solver.add_node(std::make_shared<TransportCenter>(s));
    }

    if (!tar.second) {
      t = TransportCenter{ target };
      t.set_latency<MovementType::CARTING, ProcessType::INBOUND>(DURATION(0));
      t.set_latency<MovementType::LINEHAUL, ProcessType::INBOUND>(DURATION(0));
      t.set_latency<MovementType::CARTING, ProcessType::OUTBOUND>(DURATION(0));
      t.set_latency<MovementType::LINEHAUL, ProcessType::OUTBOUND>(DURATION(0));
      tar = solver.add_node(std::make_shared<TransportCenter>(t));
    }

    solver.add_edge(src.first, tar.first, e);
  }

  app.logger().debug("Processing loads");
  while (true) {
    Poco::Thread::sleep(200);
    std::string payload;
    app.logger().debug(
      std::format("C: Queue size: {}", load_queue->size_approx()));

    if (load_queue->try_dequeue(payload)) {
      try {
        app.logger().debug(std::format("Dequeued: {}", payload));
        nlohmann::json data = nlohmann::json::parse(payload);

        std::vector<std::tuple<std::string, int32_t, std::string>> packages;

        for (auto& waybill : data["items"]) {
          if (!waybill["cpdd_destination"].is_null())
            packages.emplace_back(
              "",
              iso_to_date(
                waybill["cpdd_destination"].template get<std::string>())
                .time_since_epoch()
                .count(),
              waybill["id"].template get<std::string>());
        }

        nlohmann::json solution =
          find_paths(data["id"].template get<std::string>(),
                     data["location"].template get<std::string>(),
                     data["destination"].template get<std::string>(),
                     iso_to_date(data["time"].template get<std::string>())
                       .time_since_epoch()
                       .count(),
                     packages);

        solution["cs_slid"] = data["cs_slid"].is_null()
                                ? ""
                                : data["cs_slid"].template get<std::string>();

        solution["cs_act"] = data["cs_act"].is_null()
                               ? ""
                               : data["cs_act"].template get<std::string>();
        solution["pid"] =
          data["pid"].is_null() ? "" : data["pid"].template get<std::string>();
        solution_queue->enqueue(solution.dump());
      } catch (const std::exception& exc) {
        app.logger().error(std::format("Solver Error: {}", exc.what()));
      }
    }
  }
}
